	/* Object identity formation accepts two independently typed endpoints. It
	 * does not require that both Claims were reindexed from one source term. */
	uint32_t true_true_identity_family_claim;
	uint32_t true_false_identity_family_claim;
	if (prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			true_claim,
			&true_true_identity_family_claim
		) != 0 || prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			false_claim,
			&true_false_identity_family_claim
		) != 0) {
		return 316;
	}
	const struct prototype_judgement_proposition* true_true_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, true_true_identity_family_claim
		);
	const struct prototype_judgement_proposition* true_false_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, true_false_identity_family_claim
		);
	uint32_t true_true_identity_type_id;
	uint32_t true_false_identity_type_id;
	uint32_t true_true_identity_arguments[2];
	uint32_t true_false_identity_arguments[2];
	uint32_t true_true_identity_argument_count;
	uint32_t true_false_identity_argument_count;
	if (!true_true_identity_family || !true_false_identity_family ||
		true_true_identity_family->subject == true_false_identity_family->subject ||
		prototype_term_type_instance_info(
			&term_db,
			true_true_identity_family->subject,
			&true_true_identity_type_id,
			true_true_identity_arguments,
			&true_true_identity_argument_count
		) != 0 || prototype_term_type_instance_info(
			&term_db,
			true_false_identity_family->subject,
			&true_false_identity_type_id,
			true_false_identity_arguments,
			&true_false_identity_argument_count
		) != 0 || true_true_identity_type_id !=
			bool_identity_certificate->data.identity_type.
				generated_type_declaration_id ||
		true_false_identity_type_id != true_true_identity_type_id ||
		true_true_identity_argument_count != 2 ||
		true_false_identity_argument_count != 2 ||
		true_true_identity_arguments[0] != bool_true ||
		true_true_identity_arguments[1] != bool_true ||
		true_false_identity_arguments[0] != bool_true ||
		true_false_identity_arguments[1] != bool_false) {
		return 317;
	}
	uint32_t true_degeneracy_family_claim;
	uint32_t true_degeneracy_witness;
	uint32_t true_degeneracy_witness_claim;
	uint32_t checked_true_identity_family_claim;
	uint32_t checked_true_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			&true_degeneracy_family_claim,
			&true_degeneracy_witness,
			&true_degeneracy_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			true_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			false_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) == 0) {
		return 318;
	}
	(void)true_degeneracy_family_claim;
	(void)true_degeneracy_witness;
	struct prototype_hott_action_request universe_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t universe_identity_computation_id;
	uint32_t universe_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_identity_computation,
			&universe_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_identity_computation_id,
			&universe_identity_computation_result_id
		) != 0 || action_results[
			universe_identity_computation_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[universe_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 268;
	}
	const struct prototype_hott_action_certificate*
		universe_identity_certificate = &action_db.certificates[
			action_results[
				universe_identity_computation_result_id
			].certificate_id
		];
	uint32_t universe_identity_type_id = universe_identity_certificate->
		data.identity_type.generated_type_declaration_id;
	if (universe_identity_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		universe_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE ||
		universe_identity_type_id >= type_db.type_count ||
		type_db.type_declarations[universe_identity_type_id].index_count != 2 ||
		type_db.type_declarations[universe_identity_type_id].constructor_count != 1 ||
		!prototype_type_declaration_validate_generated_identity(
			&term_db,
			&type_db,
			&context_db,
			universe,
			universe_identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE
		) || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 269;
	}

	/* Type-family functions inhabit an ordinary Pi. Their identity is the
	 * same pointwise rule as for CBPV functions, without Thunk/Comp wrappers. */
	uint32_t type_family_binding = prototype_term_new_binding(&term_db);
	uint32_t type_family_codomain;
	uint32_t type_family_pi;
	uint32_t type_family_pi_has_type;
	uint32_t type_family_pi_is_type;
	if (type_family_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			&term_db, type_family_binding, universe, &type_family_codomain
		) != 0 || prototype_term_pi_family(
			&term_db, bool_view, type_family_codomain, &type_family_pi
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			type_family_pi,
			universe,
			&type_family_pi_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			type_family_pi,
			universe,
			type_family_pi_has_type,
			&type_family_pi_is_type
		) != 0) {
		return 283;
	}
	struct prototype_hott_action_request type_family_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = type_family_pi_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t type_family_identity_request;
	uint32_t type_family_identity_result;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			type_family_identity_computation,
			&type_family_identity_request
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			type_family_identity_request,
			&type_family_identity_result
		) != 0 || action_results[type_family_identity_result].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[type_family_identity_result].certificate_id >=
			action_db.certificate_count) {
		return 284;
	}
	const struct prototype_hott_identity_type_computation_certificate*
		type_family_identity = &action_db.certificates[
			action_results[type_family_identity_result].certificate_id
		].data.identity_type;
	uint32_t type_family_identity_term = type_family_identity->identity_type_term_id;
	for (uint32_t i = 0; i < 3; ++i) {
		uint32_t ignored_binding;
		if (type_family_identity_term >= term_db.term_count ||
			term_db.terms[type_family_identity_term].tag != PROTOTYPE_TERM_PI ||
			prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[type_family_identity_term].as.pi.codomain_family,
				&ignored_binding,
				&type_family_identity_term
			) != 0) {
			return 285;
		}
	}
	if (type_family_identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		return 286;
	}
	uint32_t type_family_result_type_id;
	uint32_t type_family_result_arguments[2];
	uint32_t type_family_result_argument_count;
	if (prototype_term_type_instance_info(
			&term_db,
			type_family_identity_term,
			&type_family_result_type_id,
			type_family_result_arguments,
			&type_family_result_argument_count
		) != 0 || type_family_result_type_id != universe_identity_type_id ||
		type_family_result_argument_count != 2) {
		return 287;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 288;
	}

	/* Raw type-level Lambda uses the same pointwise action as a CBPV
	 * function, but its witness remains in the value/type fragment. */
	uint32_t constant_type_family_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_type_family_context;
	uint32_t constant_type_family_context_certificate;
	uint32_t constant_type_family_var_claim;
	uint32_t constant_type_family_projection;
	uint32_t constant_type_family_body_claim;
	uint32_t constant_type_family_lambda;
	uint32_t constant_type_family_lambda_claim;
	if (constant_type_family_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			constant_type_family_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_type_family_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_type_family_context,
			bool_is_type,
			&constant_type_family_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_type_family_context,
			constant_type_family_binding,
			bool_view,
			&constant_type_family_var_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_type_family_context,
			empty,
			&constant_type_family_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			constant_type_family_projection,
			&constant_type_family_body_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_type_family_binding,
			bool_view,
			&constant_type_family_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_type_family_lambda,
			type_family_pi,
			constant_type_family_var_claim,
			constant_type_family_body_claim,
			&constant_type_family_lambda_claim
		) != 0) {
		return 313;
	}
	uint32_t constant_type_family_identity_family_claim;
	uint32_t constant_type_family_witness;
	uint32_t constant_type_family_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			type_family_identity_result,
			constant_type_family_lambda_claim,
			&constant_type_family_identity_family_claim,
			&constant_type_family_witness,
			&constant_type_family_witness_claim
		) != 0 ||
		constant_type_family_witness >= term_db.term_count ||
		term_db.terms[constant_type_family_witness].tag != PROTOTYPE_TERM_LAMBDA) {
		return 314;
	}
	const struct prototype_judgement_proposition*
		constant_type_family_identity_family =
			prototype_judgement_claim_proposition(
				&judgement, constant_type_family_identity_family_claim
			);
	const struct prototype_judgement_proposition* constant_type_family_witness_type =
		prototype_judgement_claim_proposition(
			&judgement, constant_type_family_witness_claim
		);
	if (!constant_type_family_identity_family ||
		!constant_type_family_witness_type ||
		constant_type_family_witness_type->classifier !=
			constant_type_family_identity_family->subject ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 315;
	}
	(void)constant_type_family_context_certificate;

	/* A family variable and its argument are related in the object bridge.
	 * APP must apply the family witness, not compare the resulting type Terms. */
	uint32_t family_value_binding = prototype_term_new_binding(&term_db);
	uint32_t family_value_context;
	uint32_t family_value_context_certificate;
	uint32_t family_value_bridge;
	int family_value_residual;
	if (family_value_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			family_value_binding,
			type_family_pi,
			PROTOTYPE_INVALID_ID,
			&family_value_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_value_context,
			type_family_pi_is_type,
			&family_value_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_value_context,
			&family_value_bridge,
			&family_value_residual
		) != 0 || family_value_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 289;
	}
	uint32_t family_to_empty;
	uint32_t bool_in_family_context;
	uint32_t family_argument_binding = prototype_term_new_binding(&term_db);
	uint32_t family_argument_context;
	uint32_t family_argument_context_certificate;
	uint32_t family_argument_bridge;
	int family_argument_residual;
	if (family_argument_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			family_value_context,
			empty,
			&family_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			family_to_empty,
			&bool_in_family_context
		) != 0 || prototype_context_extend(
			&context_db,
			family_value_context,
			family_argument_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&family_argument_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_argument_context,
			bool_in_family_context,
			&family_argument_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_argument_context,
			&family_argument_bridge,
			&family_argument_residual
		) != 0 || family_argument_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 290;
	}
	uint32_t family_value_var;
	uint32_t family_argument_var;
	uint32_t family_value_claim;
	uint32_t family_argument_claim;
	uint32_t family_application;
	uint32_t family_application_claim;
	if (prototype_term_var(
			&term_db, family_value_binding, &family_value_var
		) != 0 || prototype_term_var(
			&term_db, family_argument_binding, &family_argument_var
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			family_argument_context,
			family_value_binding,
			type_family_pi,
			&family_value_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			family_argument_context,
			family_argument_binding,
			bool_view,
			&family_argument_claim
		) != 0 || prototype_term_app(
			&term_db,
			family_value_var,
			family_argument_var,
			&family_application
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			family_argument_context,
			family_application,
			universe,
			family_value_claim,
			family_argument_claim,
			&family_application_claim
		) != 0) {
		return 291;
	}
	uint32_t family_argument_to_empty;
	uint32_t universe_in_family_argument_context;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			family_argument_context,
			empty,
			&family_argument_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_is_type,
			family_argument_to_empty,
			&universe_in_family_argument_context
		) != 0) {
		return 292;
	}
	struct prototype_hott_action_request family_result_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_in_family_argument_context,
			.source_bridge_id = family_argument_bridge
		}
	};
	uint32_t family_result_identity_request_id;
	uint32_t family_result_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_result_identity_request,
			&family_result_identity_request_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_result_identity_request_id,
			&family_result_identity_result_id
		) != 0 || action_results[
			family_result_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 293;
	}
	struct prototype_hott_action_request family_application_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = family_application_claim,
			.source_bridge_id = family_argument_bridge,
			.identity_type_action_request_id =
				family_result_identity_request_id
		}
	};
	uint32_t family_application_action_id;
	uint32_t family_application_action_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_application_action,
			&family_application_action_id
		) != 0) {
		return 294;
	}
	if (prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_application_action_id,
			&family_application_action_result_id
		) != 0) {
		return 295;
	}
	if (action_results[
			family_application_action_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 296;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 297;
	}
	uint32_t family_application_is_type;
	uint32_t family_result_binding = prototype_term_new_binding(&term_db);
	uint32_t family_result_context;
	uint32_t family_result_context_certificate;
	uint32_t family_result_bridge;
	int family_result_residual;
	if (family_result_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			family_argument_context,
			family_application,
			universe,
			family_application_claim,
			&family_application_is_type
		) != 0) {
		return 298;
	}
	struct prototype_hott_action_request family_fiber_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = family_application_is_type,
			.source_bridge_id = family_argument_bridge
		}
	};
	uint32_t family_fiber_identity_request_id;
	uint32_t family_fiber_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			family_fiber_identity_request,
			&family_fiber_identity_request_id
		) != 0) {
		return 305;
	}
	if (prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			family_fiber_identity_request_id,
			&family_fiber_identity_result_id
		) != 0) {
		return 307;
	}
	if (action_results[family_fiber_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 306;
	}
	if (prototype_context_extend(
			&context_db,
			family_argument_context,
			family_result_binding,
			family_application,
			PROTOTYPE_INVALID_ID,
			&family_result_context
		) != 0) {
		return 300;
	}
	if (prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			family_result_context,
			family_application_is_type,
			&family_result_context_certificate
		) != 0) {
		return 301;
	}
	if (prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			family_result_context,
			&family_result_bridge,
			&family_result_residual
		) != 0) {
		return 302;
	}
	if (family_result_residual != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 303;
	}
	if (bridge_db.certificates[family_result_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[family_result_bridge].fiber_action_certificate_id >=
			action_db.certificate_count) {
		return 304;
	}
	const struct prototype_hott_action_certificate* family_result_fiber =
		&action_certificates[bridge_db.certificates[
			family_result_bridge
		].fiber_action_certificate_id];
	if (family_result_fiber->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		family_result_fiber->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 299;
	}
	(void)family_value_context_certificate;
	(void)family_argument_context_certificate;
	(void)family_result_context_certificate;
	uint32_t universe_proof_binding = prototype_term_new_binding(&term_db);
	uint32_t universe_proof_context;
	uint32_t universe_proof;
	uint32_t universe_identity_former;
	uint32_t universe_proof_context_certificate;
	uint32_t universe_proof_claim;
	uint32_t universe_endpoint_path[2];
	uint32_t universe_endpoint_count;
	uint32_t universe_left_endpoint_claim;
	uint32_t universe_right_endpoint_claim;
	uint32_t universe_left_in_proof_context;
	uint32_t universe_right_in_proof_context;
	uint32_t universe_proof_to_left;
	uint32_t universe_proof_to_right;
	if (universe_proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extension_path(
			&context_db,
			empty,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_endpoint_path,
			2,
			&universe_endpoint_count
		) != 0 || universe_endpoint_count != 2 ||
		prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_endpoint_path[0],
			universe_identity_certificate->data.identity_type.
				left_endpoint_binding_id,
			universe,
			&universe_left_endpoint_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_endpoint_path[1],
			universe_identity_certificate->data.identity_type.
				right_endpoint_binding_id,
			universe,
			&universe_right_endpoint_claim
		) != 0 ||
		prototype_context_extend(
			&context_db,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_proof_binding,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			PROTOTYPE_INVALID_ID,
			&universe_proof_context
		) != 0 || prototype_term_var(
			&term_db, universe_proof_binding, &universe_proof
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			universe_identity_type_id,
			NULL,
			0,
			&universe_identity_former
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			universe_proof_context,
			universe_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			&universe_proof_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			universe_proof_context,
			universe_proof_binding,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			&universe_proof_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_proof_context,
			universe_endpoint_path[0],
			&universe_proof_to_left
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_proof_context,
			universe_endpoint_path[1],
			&universe_proof_to_right
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_left_endpoint_claim,
			universe_proof_to_left,
			&universe_left_in_proof_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_right_endpoint_claim,
			universe_proof_to_right,
			&universe_right_in_proof_context
		) != 0) {
		return 271;
	}
	struct prototype_case_binder universe_branch_binders[7];
	uint32_t universe_branch_fields[7];
	uint32_t universe_branch_context = universe_proof_context;
	for (uint32_t i = 0; i < 7; ++i) {
		uint32_t classifier;
		uint32_t next_context;
		universe_branch_binders[i] = (struct prototype_case_binder) {
			.binding_id = prototype_term_new_binding(&term_db),
			.is_recursive = 0
		};
		if (universe_branch_binders[i].binding_id == PROTOTYPE_INVALID_ID ||
			prototype_judgement_constructor_field_classifier(
				&term_db,
				&type_db,
				&context_db,
				&substitution_db,
				universe_branch_context,
				universe_identity_former,
				0,
				universe_branch_binders,
				i,
				i,
				&classifier
			) != 0 || prototype_context_extend(
				&context_db,
				universe_branch_context,
				universe_branch_binders[i].binding_id,
				classifier,
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				&term_db,
				universe_branch_binders[i].binding_id,
				&universe_branch_fields[i]
			) != 0) {
			return 271;
		}
		universe_branch_context = next_context;
	}
	uint32_t refined_universe_branch_context;
	uint32_t universe_branch_substitution;
	uint32_t refined_universe_constructor;
	if (prototype_judgement_identity_relation_branch_refinement(
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			universe_proof_context,
			universe_proof,
			universe_identity_certificate->data.identity_type.identity_type_term_id,
			0,
			universe_branch_context,
			universe_branch_binders,
			7,
			&refined_universe_branch_context,
			&universe_branch_substitution,
			&refined_universe_constructor
		) != 0 || prototype_substitution_get(
			&substitution_db, universe_branch_substitution
		)->source_context != refined_universe_branch_context ||
		prototype_substitution_get(
			&substitution_db, universe_branch_substitution
		)->target_context != universe_branch_context) {
		return 271;
	}
	uint32_t refined_universe_left;
	uint32_t refined_universe_right;
	uint32_t refined_universe_proof;
	uint32_t preserved_universe_transport;
	uint32_t universe_left_endpoint;
	uint32_t universe_right_endpoint;
	if (prototype_term_var(
			&term_db,
			universe_identity_certificate->data.identity_type.left_endpoint_binding_id,
			&universe_left_endpoint
		) != 0 || prototype_term_var(
			&term_db,
			universe_identity_certificate->data.identity_type.right_endpoint_binding_id,
			&universe_right_endpoint
		) != 0 || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_left_endpoint,
			universe_branch_substitution,
			&refined_universe_left
		) != 0 || refined_universe_left != universe_branch_fields[0] ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_right_endpoint,
			universe_branch_substitution,
			&refined_universe_right
		) != 0 || refined_universe_right != universe_branch_fields[1] ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_proof,
			universe_branch_substitution,
			&refined_universe_proof
		) != 0 || refined_universe_proof != refined_universe_constructor ||
		prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_branch_fields[3],
			universe_branch_substitution,
			&preserved_universe_transport
		) != 0 || preserved_universe_transport != universe_branch_fields[3]) {
		return 271;
	}
	uint32_t universe_degeneracy_family_claim;
	uint32_t universe_degeneracy_witness;
	uint32_t universe_degeneracy_witness_claim;
	int universe_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_identity_computation_result_id,
			bool_has_type,
			&universe_degeneracy_family_claim,
			&universe_degeneracy_witness,
			&universe_degeneracy_witness_claim
		);
	if (universe_degeneracy_status != 0) {
		fprintf(
			stderr,
			"universe degeneracy construction failed: %d\n",
			universe_degeneracy_status
		);
		return 270;
	}
	const struct prototype_judgement_proposition* universe_degeneracy_family =
		prototype_judgement_claim_proposition(
			&judgement, universe_degeneracy_family_claim
		);
	const struct prototype_judgement_proposition* universe_degeneracy_proof =
		prototype_judgement_claim_proposition(
			&judgement, universe_degeneracy_witness_claim
		);
	uint32_t expected_universe_identity_arguments[2] = {
		bool_view, bool_view
	};
	uint32_t expected_universe_identity;
	if (!universe_degeneracy_family || !universe_degeneracy_proof ||
		prototype_term_type_instance_make(
			&term_db,
			&type_db,
			universe_identity_type_id,
			expected_universe_identity_arguments,
			2,
			&expected_universe_identity
		) != 0 || universe_degeneracy_family->subject !=
			expected_universe_identity || universe_degeneracy_proof->subject !=
			universe_degeneracy_witness || universe_degeneracy_proof->classifier !=
			expected_universe_identity || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 271;
	}
	uint32_t universe_witness_head;
	uint32_t universe_witness_owner;
	uint32_t universe_witness_constructor;
	uint32_t universe_witness_arguments[7];
	uint32_t universe_witness_argument_count;
	if (prototype_term_constructor_spine_info(
			&term_db,
			universe_degeneracy_witness,
			&universe_witness_head,
			&universe_witness_owner,
			&universe_witness_constructor,
			universe_witness_arguments,
			7,
			&universe_witness_argument_count
		) != 0 || universe_witness_constructor != 0 ||
		universe_witness_argument_count != 7) {
		return 273;
	}
	uint32_t concrete_universe_substitution;
	if (prototype_substitution_empty(
			&substitution_db,
			&context_db,
			empty,
			&concrete_universe_substitution
		) != 0 || prototype_context_extension_path(
			&context_db,
			empty,
			universe_identity_certificate->data.identity_type.endpoint_context_id,
			universe_endpoint_path,
			2,
			&universe_endpoint_count
		) != 0 || universe_endpoint_count != 2 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_endpoint_path[0],
			bool_view,
			universe,
			&concrete_universe_substitution
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_endpoint_path[1],
			bool_view,
			universe,
			&concrete_universe_substitution
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			concrete_universe_substitution,
			universe_proof_context,
			universe_degeneracy_witness,
			expected_universe_identity,
			&concrete_universe_substitution
		) != 0) {
		return 273;
	}
	for (int projection = PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION;
		projection <= PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_LEFT;
		++projection) {
		uint32_t projected;
		uint32_t projected_claim;
		uint32_t concrete_projected;
		uint32_t normalized_projection;
		const struct prototype_judgement_proposition* projection_proposition;
		int projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				&action_db,
				&kernel_builder,
				&bridge_db,
				universe_identity_computation_result_id,
				universe_proof_claim,
				projection,
				&projected,
				&projected_claim
			);
		if (projection_status != 0) {
			return 273;
		}
		if (prototype_term_reindex(
				&term_db,
				&type_db,
				&context_db,
				&substitution_db,
				projected,
				concrete_universe_substitution,
				&concrete_projected
			) != 0 || prototype_term_normalize_complete_with_profile(
				&term_db,
				&type_db,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				concrete_projected,
				&normalized_projection
			) != 0 || normalized_projection !=
				universe_witness_arguments[projection] || !(projection_proposition =
				prototype_judgement_claim_proposition(
					&judgement, projected_claim
				)) || projection_proposition->subject != projected) {
			return 273;
		}
	}
	(void)universe_witness_head;
	(void)universe_witness_owner;
	uint32_t universe_variable_bridge;
	uint32_t universe_context_to_empty;
	uint32_t universe_is_type_in_variable_context;
	struct prototype_hott_action_request lifted_universe_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t lifted_universe_identity_id;
	uint32_t lifted_universe_identity_result_id;
	uint32_t universe_variable_identity_family_claim;
	uint32_t universe_variable_identity_witness;
	uint32_t universe_variable_identity_witness_claim;
	uint32_t universe_variable_relation;
	uint32_t universe_variable_relation_claim;
	struct prototype_hott_action_request universe_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = universe_left_endpoint_claim,
			.source_bridge_id = PROTOTYPE_INVALID_ID,
			.identity_type_action_request_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t universe_variable_action_id;
	uint32_t universe_variable_action_result_id;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			universe_endpoint_path[0],
			universe_identity_computation_id,
			&universe_variable_bridge
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_endpoint_path[0],
			empty,
			&universe_context_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_is_type,
			universe_context_to_empty,
			&universe_is_type_in_variable_context
		) != 0) {
		return 273;
	}
	lifted_universe_identity.key.identity_type.source_claim_id =
		universe_is_type_in_variable_context;
	lifted_universe_identity.key.identity_type.source_bridge_id =
		universe_variable_bridge;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			lifted_universe_identity,
			&lifted_universe_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			lifted_universe_identity_id,
			&lifted_universe_identity_result_id
		) != 0 || action_certificates[
			action_results[lifted_universe_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		return 273;
	}
	universe_variable_action.key.object_term.source_bridge_id =
		universe_variable_bridge;
	universe_variable_action.key.object_term.identity_type_action_request_id =
		lifted_universe_identity_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_variable_action,
			&universe_variable_action_id
		) != 0 || prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_variable_action_id,
			&universe_variable_action_result_id
		) != 0 || action_results[
			universe_variable_action_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			universe_variable_action_result_id
		].certificate_id >= action_db.certificate_count || action_certificates[
			action_results[universe_variable_action_result_id].certificate_id
		].kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
		return 273;
	}
	const struct prototype_hott_object_term_action_certificate*
		universe_variable_action_certificate = &action_certificates[
			action_results[universe_variable_action_result_id].certificate_id
		].data.object_term;
	universe_variable_identity_family_claim =
		universe_variable_action_certificate->identity_family_has_type_claim_id;
	universe_variable_identity_witness =
		universe_variable_action_certificate->witness_term_id;
	universe_variable_identity_witness_claim =
		universe_variable_action_certificate->witness_has_type_claim_id;
	struct prototype_hott_action_certificate forged_object_action = {
		.request_id = universe_variable_action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM,
		.data.object_term = *universe_variable_action_certificate
	};
	uint32_t forged_object_action_certificate_id;
	forged_object_action.data.object_term.identity_family_has_type_claim_id =
		universe_left_endpoint_claim;
	if (prototype_hott_action_certificate_add(
			&action_db,
			&kernel_view,
			&bridge_db,
			forged_object_action,
			&forged_object_action_certificate_id
		) == 0) {
		return 273;
	}
	if (prototype_hott_construct_universe_correspondence_projection(
			&action_db,
			&kernel_builder,
			&bridge_db,
			lifted_universe_identity_result_id,
			universe_variable_identity_witness_claim,
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
			&universe_variable_relation,
			&universe_variable_relation_claim
		) != 0 || !prototype_judgement_claim_proposition(
			&judgement, universe_variable_relation_claim
		) || prototype_judgement_claim_proposition(
			&judgement, universe_variable_relation_claim
		)->subject != universe_variable_relation) {
		return 273;
	}
	(void)universe_variable_identity_family_claim;
	(void)universe_variable_identity_witness;
	uint32_t universe_variable_is_type;
	struct prototype_hott_action_request universe_fiber_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = universe_variable_bridge
		}
	};
	uint32_t universe_fiber_identity_id;
	uint32_t universe_fiber_identity_result_id;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			universe_endpoint_path[0],
			universe_left_endpoint,
			universe,
			universe_left_endpoint_claim,
			&universe_variable_is_type
		) != 0) {
		return 273;
	}
	universe_fiber_identity.key.identity_type.source_claim_id =
		universe_variable_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			universe_fiber_identity,
			&universe_fiber_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			universe_fiber_identity_id,
			&universe_fiber_identity_result_id
		) != 0 || action_results[
			universe_fiber_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			universe_fiber_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		return 273;
	}
	const struct prototype_hott_action_certificate* universe_fiber_certificate =
		&action_certificates[action_results[
			universe_fiber_identity_result_id
		].certificate_id];
	uint32_t universe_fiber_endpoint_path[2];
	uint32_t universe_fiber_endpoint_count;
	uint32_t universe_fiber_left;
	uint32_t universe_fiber_right;
	uint32_t expected_universe_fiber_at_left;
	uint32_t expected_universe_fiber;
	if (universe_fiber_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_context_extension_path(
			&context_db,
			bridge_db.bridges[universe_variable_bridge].bridge_context_id,
			universe_fiber_certificate->data.identity_type.endpoint_context_id,
			universe_fiber_endpoint_path,
			2,
			&universe_fiber_endpoint_count
		) != 0 || universe_fiber_endpoint_count != 2 || prototype_term_var(
			&term_db,
			universe_fiber_certificate->data.identity_type.left_endpoint_binding_id,
			&universe_fiber_left
		) != 0 || prototype_term_var(
			&term_db,
			universe_fiber_certificate->data.identity_type.right_endpoint_binding_id,
			&universe_fiber_right
		) != 0 || prototype_term_app(
			&term_db,
			universe_fiber_certificate->data.identity_type.backing_type_former_term_id,
			universe_fiber_left,
			&expected_universe_fiber_at_left
		) != 0 || prototype_term_app(
			&term_db,
			expected_universe_fiber_at_left,
			universe_fiber_right,
			&expected_universe_fiber
		) != 0 || expected_universe_fiber !=
			universe_fiber_certificate->data.identity_type.identity_type_term_id ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 273;
	}
	uint32_t universe_element_binding = prototype_term_new_binding(&term_db);
	uint32_t universe_element_context;
	uint32_t universe_element_context_certificate;
	uint32_t universe_element_bridge;
	int universe_element_residual_reason;
	if (universe_element_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			universe_endpoint_path[0],
			universe_element_binding,
			universe_left_endpoint,
			PROTOTYPE_INVALID_ID,
			&universe_element_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			universe_element_context,
			universe_variable_is_type,
			&universe_element_context_certificate
		) != 0 || prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			universe_element_context,
			&universe_element_bridge,
			&universe_element_residual_reason
		) != 0 || bridge_db.certificates[
			universe_element_bridge
		].semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[universe_element_bridge].fiber_action_certificate_id !=
			action_results[universe_fiber_identity_result_id].certificate_id ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 273;
	}
	if (universe_element_residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE) {
		return 273;
	}
	(void)universe_element_context_certificate;
	uint32_t universe_type_projection;
	uint32_t universe_type_in_element_context;
	uint32_t second_universe_element_binding = prototype_term_new_binding(&term_db);
	uint32_t second_universe_element_context;
	uint32_t second_universe_element_context_certificate;
	uint32_t second_universe_element_bridge;
	int second_universe_element_residual_reason;
	if (second_universe_element_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			universe_element_context,
			universe_endpoint_path[0],
			&universe_type_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			universe_variable_is_type,
			universe_type_projection,
			&universe_type_in_element_context
		) != 0) {
		return 273;
	}
	if (prototype_context_extend(
			&context_db,
			universe_element_context,
			second_universe_element_binding,
			universe_left_endpoint,
			PROTOTYPE_INVALID_ID,
			&second_universe_element_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			second_universe_element_context,
			universe_type_in_element_context,
			&second_universe_element_context_certificate
		) != 0) {
		return 273;
	}
	if (prototype_hott_bridge_db_ensure_identity_context(
			&bridge_db,
			&kernel_builder,
			&action_db,
			second_universe_element_context,
			&second_universe_element_bridge,
			&second_universe_element_residual_reason
		) != 0) {
		return 273;
	}
	if (second_universe_element_residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NONE || bridge_db.certificates[
				second_universe_element_bridge
			].semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id >= action_db.certificate_count ||
		action_certificates[bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		action_certificates[bridge_db.certificates[
			second_universe_element_bridge
		].fiber_action_certificate_id].data.identity_type.backing_type_former_term_id !=
			universe_fiber_certificate->data.identity_type.backing_type_former_term_id ||
		prototype_hott_bridge_db_validate(&bridge_db, &kernel_view) != 0) {
		return 273;
	}
	(void)second_universe_element_context_certificate;
	uint32_t bool_identity_endpoint_path[2];
	uint32_t bool_identity_endpoint_count;
	uint32_t first_endpoint_bridge;
	if (prototype_context_extension_path(
			&context_db,
			empty,
			bool_identity_certificate->data.identity_type.endpoint_context_id,
			bool_identity_endpoint_path,
			2,
			&bool_identity_endpoint_count
		) != 0 || bool_identity_endpoint_count != 2 ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_identity_endpoint_path[0],
			bool_identity_computation_id,
			&first_endpoint_bridge
		) != 0) {
		return 251;
	}
	uint32_t first_endpoint_to_empty;
	uint32_t bool_is_type_in_first_endpoint;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			bool_identity_endpoint_path[0],
			empty,
			&first_endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			first_endpoint_to_empty,
			&bool_is_type_in_first_endpoint
		) != 0) {
		return 252;
	}
	struct prototype_hott_action_request constant_bool_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_is_type_in_first_endpoint,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_bool_identity_id;
	uint32_t constant_bool_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_bool_identity,
			&constant_bool_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_id,
			&constant_bool_identity_result_id
		) != 0 || action_results[
			constant_bool_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_certificates[
			action_results[constant_bool_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 253;
	}
	uint32_t endpoint_bool_binding = context_db.contexts[
		bool_identity_endpoint_path[0]
	].binding_id;
	uint32_t endpoint_bool_claim;
	uint32_t endpoint_bool_identity_family_claim;
	uint32_t endpoint_bool_action_witness;
	uint32_t endpoint_bool_action_witness_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			bool_identity_endpoint_path[0],
			endpoint_bool_binding,
			bool_view,
			&endpoint_bool_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_result_id,
			endpoint_bool_claim,
			&endpoint_bool_identity_family_claim,
			&endpoint_bool_action_witness,
			&endpoint_bool_action_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_action_witness_claim
		)->context_id != bridge_db.bridges[
			first_endpoint_bridge
		].bridge_context_id || endpoint_bool_action_witness !=
			prototype_judgement_claim_proposition(
				&judgement,
				bridge_certificates[first_endpoint_bridge].fiber_witness_claim_id
			)->subject || prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_action_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, endpoint_bool_identity_family_claim
		)->subject) {
		return 254;
	}
	uint32_t endpoint_to_empty;
	uint32_t endpoint_false_claim;
	uint32_t endpoint_false_identity_family_claim;
	uint32_t endpoint_false_action_witness;
	uint32_t endpoint_false_action_witness_claim;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			bool_identity_endpoint_path[0],
			empty,
			&endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_false_constructor_claim,
			endpoint_to_empty,
			&endpoint_false_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_bool_identity_result_id,
			endpoint_false_claim,
			&endpoint_false_identity_family_claim,
			&endpoint_false_action_witness,
			&endpoint_false_action_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, endpoint_false_action_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, endpoint_false_identity_family_claim
		)->subject || endpoint_false_action_witness >= term_db.term_count ||
		term_db.terms[endpoint_false_action_witness].tag !=
			PROTOTYPE_TERM_CONSTRUCTOR) {
		return 255;
	}
	uint32_t full_endpoint_bridge;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_identity_endpoint_path[1],
			constant_bool_identity_id,
			&full_endpoint_bridge
		) != 0 || context_db.contexts[
			bridge_db.bridges[full_endpoint_bridge].bridge_context_id
		].depth != 6 || prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 254;
	}
	struct prototype_hott_action_request square_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			.source_bridge_id = full_endpoint_bridge
		}
	};
	uint32_t square_identity_computation_id;
	uint32_t square_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			square_identity_computation,
			&square_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			square_identity_computation_id,
			&square_identity_result_id
		) != 0 || action_results[square_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			square_identity_result_id
		].certificate_id >= action_db.certificate_count || action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT ||
		type_db.type_declarations[action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.generated_type_declaration_id].index_count != 8 ||
		type_db.type_declarations[action_certificates[
			action_results[square_identity_result_id].certificate_id
		].data.identity_type.generated_type_declaration_id].constructor_count != 2) {
		return 255;
	}
	uint32_t generated_square_identity_type = action_certificates[
		action_results[square_identity_result_id].certificate_id
	].data.identity_type.generated_type_declaration_id;
	struct prototype_artifact_identity_root identity_roots[20];
	struct prototype_artifact_dependency identity_dependencies[16];
	struct prototype_artifact_interface identity_interface;
	prototype_artifact_interface_init(
		&identity_interface,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		identity_roots,
		sizeof(identity_roots) / sizeof(identity_roots[0]),
			identity_dependencies,
			16
		);
	uint32_t identity_root_id;
	uint32_t repeated_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&identity_root_id
		) != 0 || identity_root_id != 0 || prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&repeated_identity_root_id
		) != 0 || repeated_identity_root_id != identity_root_id ||
		identity_interface.identity_root_count != 1 ||
		identity_roots[0].identity_family_has_type_claim_id !=
			bool_identity_certificate->data.identity_type.
				identity_type_has_type_claim_id ||
		prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_relation_result_id,
			PROTOTYPE_INVALID_ID,
			&(uint32_t) { 0 }
		) == 0) {
		return 218;
	}
	uint32_t saved_identity_endpoint_context =
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].data.identity_type.endpoint_context_id;
	action_db.certificates[
		action_results[bool_identity_computation_result_id].certificate_id
	].data.identity_type.endpoint_context_id = empty;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 168;
	}
	action_db.certificates[
		action_results[bool_identity_computation_result_id].certificate_id
	].data.identity_type.endpoint_context_id = saved_identity_endpoint_context;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 169;
	}
	uint32_t generated_bool_identity_type =
		bool_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	uint32_t false_identity_args[2] = { bool_false, bool_false };
	uint32_t false_true_identity_args[2] = { bool_false, bool_true };
	uint32_t false_identity_type;
	uint32_t false_true_identity_type;
	uint32_t false_identity_witness;
	uint32_t false_true_invalid_witness;
	uint32_t false_identity_type_claim;
	uint32_t false_identity_witness_claim;
	uint32_t ignored_false_true_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			false_identity_args,
			2,
			&false_identity_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			false_true_identity_args,
			2,
			&false_true_identity_type
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			bool_false_constructor_claim,
			&false_identity_type_claim,
			&false_identity_witness,
			&false_identity_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, false_identity_type_claim
		)->subject != false_identity_type || prototype_judgement_claim_proposition(
			&judgement, false_identity_witness_claim
		)->classifier != false_identity_type || prototype_term_constructor(
			&term_db,
			false_true_identity_type,
			0,
			&false_true_invalid_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_true_invalid_witness,
			false_true_identity_type,
			&ignored_false_true_claim
		) == 0) {
		return 141;
	}
	uint32_t false_square_boundary[8] = {
		bool_false,
		bool_false,
		false_identity_witness,
		bool_false,
		bool_false,
		false_identity_witness,
		false_identity_witness,
		false_identity_witness
	};
	uint32_t mixed_square_boundary[8];
	memcpy(
		mixed_square_boundary,
		false_square_boundary,
		sizeof(false_square_boundary)
	);
	mixed_square_boundary[4] = bool_true;
	uint32_t false_square_type;
	uint32_t mixed_square_type;
	uint32_t false_square_witness;
	uint32_t mixed_square_witness;
	uint32_t false_square_claim;
	uint32_t ignored_mixed_square_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_square_identity_type,
			false_square_boundary,
			8,
			&false_square_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_square_identity_type,
			mixed_square_boundary,
			8,
			&mixed_square_type
		) != 0 || prototype_term_constructor(
			&term_db, false_square_type, 0, &false_square_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_square_witness,
			false_square_type,
			&false_square_claim
		) != 0 || prototype_term_constructor(
			&term_db, mixed_square_type, 0, &mixed_square_witness
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			mixed_square_witness,
			mixed_square_type,
			&ignored_mixed_square_claim
		) == 0) {
		return 246;
	}
	struct prototype_hott_action_request higher_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = false_identity_type_claim,
			.source_bridge_id = bridge
		}
	};
	uint32_t higher_identity_computation_id;
	uint32_t higher_identity_result_id;
	uint32_t false_identity_is_type;
	uint32_t higher_identity_family_claim = PROTOTYPE_INVALID_ID;
	uint32_t higher_identity_witness = PROTOTYPE_INVALID_ID;
	uint32_t higher_identity_witness_claim = PROTOTYPE_INVALID_ID;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			false_identity_type,
			universe,
			false_identity_type_claim,
			&false_identity_is_type
		) != 0) {
		return 245;
	}
	higher_identity_computation.key.identity_type.source_claim_id =
		false_identity_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			higher_identity_computation,
			&higher_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			higher_identity_computation_id,
			&higher_identity_result_id
		) != 0 || action_results[higher_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[higher_identity_result_id].certificate_id >=
			action_db.certificate_count || action_db.certificates[
			action_results[higher_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT) {
		fprintf(
			stderr,
			"higher identity failed result=%u results=%zu state=%d reason=%d "
			"certificate=%u certificates=%zu rule=%d\n",
			higher_identity_result_id,
			action_db.result_count,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].outcome.state : -1,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].outcome.residual_reason : -1,
			higher_identity_result_id < action_db.result_count ?
				action_results[higher_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			action_db.certificate_count,
			higher_identity_result_id < action_db.result_count &&
				action_results[higher_identity_result_id].certificate_id <
					action_db.certificate_count ? action_db.certificates[
					action_results[higher_identity_result_id].certificate_id
				].data.identity_type.computation_rule : -1
		);
		return 245;
	}
	int higher_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			higher_identity_result_id,
			false_identity_witness_claim,
			&higher_identity_family_claim,
			&higher_identity_witness,
			&higher_identity_witness_claim
		);
	if (higher_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, higher_identity_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, higher_identity_family_claim
		)->subject || higher_identity_witness >= term_db.term_count ||
		term_db.terms[higher_identity_witness].tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		fprintf(
			stderr,
			"higher degeneracy failed status=%d family=%u witness=%u claim=%u\n",
			higher_degeneracy_status,
			higher_identity_family_claim,
			higher_identity_witness,
			higher_identity_witness_claim
		);
		return 245;
	}
	uint32_t concrete_identity_root_id;
	if (prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_is_type,
			false_identity_type_claim,
			false_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_identity_root_id
		) != 0 || concrete_identity_root_id != 1 ||
		identity_interface.identity_root_count != 2) {
		return 220;
	}
	uint32_t empty_effect_row;
	uint32_t pure_bool_computation_type;
	uint32_t thunk_bool_type;
	uint32_t thunk_bool_has_type;
	uint32_t thunk_bool_is_type;
	if (prototype_term_effect_row_empty(&term_db, &empty_effect_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, bool_view, &pure_bool_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db, pure_bool_computation_type, &thunk_bool_type
		) != 0) {
		return 176;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			&judgement, &term_db, &type_db, &context_db, &substitution_db,
			empty, thunk_bool_type, universe, &thunk_bool_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement, &term_db, empty, thunk_bool_type, universe,
			thunk_bool_has_type, &thunk_bool_is_type
		) != 0) {
		return 236;
	}
	struct prototype_hott_action_request thunk_bool_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t thunk_bool_identity_computation_id;
	uint32_t thunk_bool_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_bool_identity_computation,
			&thunk_bool_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_bool_identity_computation_id,
			&thunk_bool_identity_result_id
		) != 0 || action_results[thunk_bool_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[thunk_bool_identity_result_id].certificate_id >=
			action_db.certificate_count) {
		return 177;
	}
	const struct prototype_hott_action_certificate* thunk_bool_identity_certificate =
		&action_db.certificates[
			action_results[thunk_bool_identity_result_id].certificate_id
		];
	uint32_t generated_thunk_bool_identity_type =
		thunk_bool_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	const struct prototype_type_declaration* generated_thunk_bool_identity =
		&type_db.type_declarations[generated_thunk_bool_identity_type];
	const struct prototype_type_constructor_declaration*
		generated_thunk_bool_return_identity = &type_db.constructor_declarations[
			generated_thunk_bool_identity->first_constructor
		];
	if (thunk_bool_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN ||
		generated_thunk_bool_identity->constructor_count != 1 ||
		prototype_context_get(
			&context_db, generated_thunk_bool_return_identity->field_context
		)->depth != 3 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 178;
	}
	uint32_t false_return;
	uint32_t true_return;
	uint32_t false_thunk;
	uint32_t true_thunk;
	uint32_t thunk_false_identity_type;
	uint32_t thunk_false_true_identity_type;
	uint32_t thunk_false_identity_arguments[2];
	uint32_t thunk_false_true_identity_arguments[2];
	uint32_t thunk_false_identity_witness;
	uint32_t false_return_claim;
	uint32_t false_thunk_claim;
	if (prototype_term_return(
			&term_db, bool_false, &false_return
		) != 0 || prototype_term_return(
			&term_db, bool_true, &true_return
		) != 0 || prototype_term_thunk(
			&term_db, false_return, &false_thunk
		) != 0 || prototype_term_thunk(
			&term_db, true_return, &true_thunk
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_return,
			pure_bool_computation_type,
			bool_false_constructor_claim,
			&false_return_claim
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			false_thunk,
			thunk_bool_type,
			false_return_claim,
			&false_thunk_claim
		) != 0) {
		return 179;
	}
	thunk_false_identity_arguments[0] = false_thunk;
	thunk_false_identity_arguments[1] = false_thunk;
	thunk_false_true_identity_arguments[0] = false_thunk;
	thunk_false_true_identity_arguments[1] = true_thunk;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_thunk_bool_identity_type,
			thunk_false_identity_arguments,
			2,
			&thunk_false_identity_type
		) != 0 || prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_thunk_bool_identity_type,
			thunk_false_true_identity_arguments,
			2,
			&thunk_false_true_identity_type
		) != 0 || prototype_term_constructor(
			&term_db,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			0,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			bool_false,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			bool_false,
			&thunk_false_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			thunk_false_identity_witness,
			false_identity_witness,
			&thunk_false_identity_witness
		) != 0) {
		return 180;
	}
	struct prototype_judgement_proposition thunk_delta_propositions[8];
	struct prototype_judgement_derivation_candidate thunk_delta_candidates[8];
	struct prototype_judgement_candidate_premise thunk_delta_premises[
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result thunk_delta_motives[1];
	struct prototype_judgement_computation_constraint thunk_delta_constraints[1];
	struct prototype_judgement_effect_row_constraint thunk_delta_effect_rows[1];
	struct prototype_judgement_delta thunk_delta;
	prototype_judgement_delta_init(
		&thunk_delta,
		&judgement,
		thunk_delta_propositions,
		thunk_delta_candidates,
		8,
		thunk_delta_premises,
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		thunk_delta_motives,
		1,
		thunk_delta_constraints,
		1,
		thunk_delta_effect_rows,
		1
	);
	prototype_judgement_delta_set_context_store(
		&thunk_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_context(&thunk_delta, empty);
	struct prototype_judgement_selected_evidence thunk_argument_evidence[3];
	uint32_t thunk_argument_occurrences[3] = {
		false_operation, false_operation, PROTOTYPE_INVALID_ID
	};
	if (prototype_judgement_delta_select_evidence(
			&thunk_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&thunk_argument_evidence[0]
		) != 0 || prototype_judgement_delta_select_evidence(
			&thunk_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&thunk_argument_evidence[1]
		) != 0 || prototype_judgement_delta_select_evidence(
			&thunk_delta,
			PROTOTYPE_INVALID_ID,
			empty,
			false_identity_witness,
			false_identity_type,
			&thunk_argument_evidence[2]
		) != 0 || prototype_judgement_delta_record_constructor_spine(
			&thunk_delta,
			&term_db,
			&type_db,
			thunk_false_identity_witness,
			thunk_false_identity_type,
			thunk_argument_occurrences,
			thunk_argument_evidence,
			3
		) != 0 || thunk_delta.proposition_count != 1 ||
		thunk_delta.propositions[0].classifier != thunk_false_identity_type ||
		prototype_judgement_delta_record_constructor_spine(
			&thunk_delta,
			&term_db,
			&type_db,
			thunk_false_identity_witness,
			thunk_false_true_identity_type,
			thunk_argument_occurrences,
			thunk_argument_evidence,
			3
		) == 0) {
		return 181;
	}
	uint32_t saved_thunk_result =
		generated_thunk_bool_return_identity->result_classifier;
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = thunk_false_true_identity_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 182;
	}
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = saved_thunk_result;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 183;
	}
	uint32_t thunk_degeneracy_family_claim;
	uint32_t thunk_degeneracy_witness;
	uint32_t thunk_degeneracy_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_bool_identity_result_id,
			false_thunk_claim,
			&thunk_degeneracy_family_claim,
			&thunk_degeneracy_witness,
			&thunk_degeneracy_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, thunk_degeneracy_family_claim
		)->subject != thunk_false_identity_type ||
		prototype_judgement_claim_proposition(
			&judgement, thunk_degeneracy_witness_claim
		)->classifier != thunk_false_identity_type ||
		thunk_degeneracy_witness != thunk_false_identity_witness) {
		return 222;
	}
	int found_thunk_constructor_spine = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				thunk_degeneracy_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION &&
			judgement.derivations[i].premise_count == 3) {
			found_thunk_constructor_spine = 1;
			break;
		}
	}
	if (!found_thunk_constructor_spine) {
		return 223;
	}
	uint32_t thunk_bool_is_type_in_endpoint;
	struct prototype_hott_action_request constant_thunk_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_thunk_identity_computation_id;
	uint32_t constant_thunk_identity_result_id;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_is_type,
			first_endpoint_to_empty,
			&thunk_bool_is_type_in_endpoint
		) != 0) {
		return 264;
	}
	constant_thunk_identity_computation.key.identity_type.source_claim_id =
		thunk_bool_is_type_in_endpoint;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_thunk_identity_computation,
			&constant_thunk_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_thunk_identity_computation_id,
			&constant_thunk_identity_result_id
		) != 0 || action_results[
			constant_thunk_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[constant_thunk_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 265;
	}
	uint32_t false_return_in_endpoint;
	uint32_t false_return_in_endpoint_claim;
	uint32_t false_thunk_in_endpoint;
	uint32_t false_thunk_in_endpoint_claim;
	uint32_t constant_thunk_family_claim;
	uint32_t constant_thunk_witness;
	uint32_t constant_thunk_witness_claim;
	if (prototype_term_return(
			&term_db, bool_false, &false_return_in_endpoint
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			false_return_in_endpoint,
			pure_bool_computation_type,
			endpoint_false_claim,
			&false_return_in_endpoint_claim
		) != 0 || prototype_term_thunk(
			&term_db, false_return_in_endpoint, &false_thunk_in_endpoint
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			false_thunk_in_endpoint,
			thunk_bool_type,
			false_return_in_endpoint_claim,
			&false_thunk_in_endpoint_claim
		) != 0) {
		return 266;
	}
	int constant_thunk_action_status = prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_thunk_identity_result_id,
			false_thunk_in_endpoint_claim,
			&constant_thunk_family_claim,
			&constant_thunk_witness,
			&constant_thunk_witness_claim
		);
	if (constant_thunk_action_status != 0 || constant_thunk_witness >=
			term_db.term_count || term_db.terms[
			constant_thunk_witness
		].tag != PROTOTYPE_TERM_APP || constant_thunk_witness !=
			thunk_degeneracy_witness || prototype_judgement_claim_proposition(
			&judgement, constant_thunk_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, constant_thunk_family_claim
		)->subject) {
		fprintf(
			stderr,
			"constant thunk action failed status=%d family=%u witness=%u claim=%u\n",
			constant_thunk_action_status,
			constant_thunk_family_claim,
			constant_thunk_witness,
			constant_thunk_witness_claim
		);
		return 266;
	}
	uint32_t thunk_identity_root_id;
	uint32_t concrete_thunk_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_bool_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&thunk_identity_root_id
		) != 0 || thunk_identity_root_id != 2 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_is_type,
			thunk_degeneracy_family_claim,
			thunk_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			&concrete_thunk_identity_root_id
		) != 0 || concrete_thunk_identity_root_id != 3 ||
		prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 228;
	}
	uint32_t artifact_saved_thunk_result = type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier;
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = thunk_false_true_identity_type;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) == 0) {
		return 230;
	}
	type_db.constructor_declarations[
		generated_thunk_bool_identity->first_constructor
	].result_classifier = artifact_saved_thunk_result;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 231;
	}
	uint32_t bool_function_codomain_binding = prototype_term_new_binding(&term_db);
	uint32_t bool_function_codomain_family;
	uint32_t bool_function_computation_type;
	uint32_t thunk_bool_function_type;
	uint32_t thunk_bool_function_has_type;
	uint32_t thunk_bool_function_is_type;
	if (bool_function_codomain_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			&term_db,
			bool_function_codomain_binding,
			pure_bool_computation_type,
			&bool_function_codomain_family
		) != 0 || prototype_term_pi_family(
			&term_db,
			bool_view,
			bool_function_codomain_family,
			&bool_function_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db,
			bool_function_computation_type,
			&thunk_bool_function_type
		) != 0) {
		return 184;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			&judgement, &term_db, &type_db, &context_db, &substitution_db,
			empty, thunk_bool_function_type, universe,
			&thunk_bool_function_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement, &term_db, empty, thunk_bool_function_type, universe,
			thunk_bool_function_has_type, &thunk_bool_function_is_type
		) != 0) {
		return 238;
	}
	uint32_t identity_function_binding = prototype_term_new_binding(&term_db);
	uint32_t identity_function_context;
	uint32_t identity_function_context_certificate;
	uint32_t identity_function_var;
	uint32_t identity_function_var_claim;
	uint32_t identity_function_return;
	uint32_t identity_function_return_claim;
	uint32_t identity_function_lambda;
	uint32_t identity_function_lambda_claim;
	uint32_t identity_function_value;
	uint32_t identity_function_value_claim;
	if (identity_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			identity_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&identity_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			identity_function_context,
			bool_is_type,
			&identity_function_context_certificate
		) != 0 || prototype_term_var(
			&term_db, identity_function_binding, &identity_function_var
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			identity_function_context,
			identity_function_binding,
			bool_view,
			&identity_function_var_claim
		) != 0 || prototype_term_return(
			&term_db, identity_function_var, &identity_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			identity_function_context,
			identity_function_return,
			pure_bool_computation_type,
			identity_function_var_claim,
			&identity_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			identity_function_binding,
			identity_function_return,
			&identity_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			identity_function_lambda,
			bool_function_computation_type,
			identity_function_var_claim,
			identity_function_return_claim,
			&identity_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, identity_function_lambda, &identity_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			identity_function_value,
			thunk_bool_function_type,
			identity_function_lambda_claim,
			&identity_function_value_claim
		) != 0) {
		return 224;
	}
	struct prototype_hott_action_request bool_function_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_bool_function_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_function_identity_computation_id;
	uint32_t bool_function_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_function_identity_computation,
			&bool_function_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_computation_id,
			&bool_function_identity_result_id
		) != 0 || action_results[bool_function_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[bool_function_identity_result_id].certificate_id >=
			action_db.certificate_count) {
		return 185;
	}
	struct prototype_hott_action_certificate* bool_function_identity_certificate =
		&action_db.certificates[
			action_results[bool_function_identity_result_id].certificate_id
		];
	if (bool_function_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE ||
		bool_function_identity_certificate->data.identity_type.
			generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
		bool_function_identity_certificate->data.identity_type.identity_type_term_id >=
			term_db.term_count || term_db.terms[
			bool_function_identity_certificate->data.identity_type.identity_type_term_id
		].tag != PROTOTYPE_TERM_THUNK_TYPE || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 186;
	}
	/* The constant Match is judgementally equal to its branch body under the
	 * exhaustive constant-eliminator rule. This fixture checks that the Pi
	 * identity family observes the same conversion rule. */
	uint32_t constant_function_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_function_context;
	uint32_t constant_function_context_certificate;
	uint32_t constant_function_argument_claim;
	uint32_t constant_function_projection;
	uint32_t constant_true_claim;
	uint32_t pure_bool_has_type_in_constant_context;
	uint32_t pure_bool_is_type_in_constant_context;
	uint32_t constant_true_return;
	uint32_t constant_true_return_claim;
	if (constant_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			constant_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_function_context,
			bool_is_type,
			&constant_function_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_function_context,
			constant_function_binding,
			bool_view,
			&constant_function_argument_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_function_context,
			empty,
			&constant_function_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			true_claim,
			constant_function_projection,
			&constant_true_claim
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			pure_bool_computation_type,
			universe,
			&pure_bool_has_type_in_constant_context
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			constant_function_context,
			pure_bool_computation_type,
			universe,
			pure_bool_has_type_in_constant_context,
			&pure_bool_is_type_in_constant_context
		) != 0 || prototype_term_return(
			&term_db, bool_true, &constant_true_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			constant_function_context,
			constant_true_return,
			pure_bool_computation_type,
			constant_true_claim,
			&constant_true_return_claim
		) != 0) {
		return 319;
	}
	uint32_t constant_function_lambda;
	uint32_t constant_function_lambda_claim;
	uint32_t constant_function_value;
	uint32_t constant_function_value_claim;
	if (prototype_term_lambda(
			&term_db,
			constant_function_binding,
			constant_true_return,
			&constant_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_function_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			constant_true_return_claim,
			&constant_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, constant_function_lambda, &constant_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			constant_function_value,
			thunk_bool_function_type,
			constant_function_lambda_claim,
			&constant_function_value_claim
		) != 0) {
		return 320;
	}
	uint32_t constant_function_argument;
	if (prototype_term_var(
			&term_db, constant_function_binding, &constant_function_argument
		) != 0) {
		return 321;
	}
	struct prototype_match_case_input constant_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = constant_true_return
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = constant_true_return
		}
	};
	uint32_t matched_constant_body;
	uint32_t matched_constant_body_claim;
	uint32_t constant_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t constant_motive_context;
	uint32_t constant_motive_context_certificate;
	uint32_t constant_motive_projection;
	uint32_t bool_is_type_in_constant_context;
	uint32_t constant_motive_binder_claim;
	uint32_t pure_bool_has_type_in_motive_context;
	uint32_t constant_motive;
	uint32_t constant_motive_classifier;
	uint32_t constant_motive_claim;
	uint32_t matched_constant_classifier;
	uint32_t matched_constant_classifier_claim;
	uint32_t matched_constant_classifier_is_type_claim;
	uint32_t constant_branch_claims[2] = {
		constant_true_return_claim,
		constant_true_return_claim
	};
	if (constant_motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			constant_function_context,
			constant_motive_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&constant_motive_context
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			constant_function_projection,
			&bool_is_type_in_constant_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			constant_motive_context,
			bool_is_type_in_constant_context,
			&constant_motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			constant_motive_context,
			constant_function_context,
			&constant_motive_projection
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			constant_motive_context,
			constant_motive_binding,
			bool_view,
			&constant_motive_binder_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_bool_has_type_in_constant_context,
			constant_motive_projection,
			&pure_bool_has_type_in_motive_context
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_motive_binding,
			pure_bool_computation_type,
			&constant_motive
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			universe,
			&constant_motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			constant_motive,
			constant_motive_classifier,
			constant_motive_binder_claim,
			pure_bool_has_type_in_motive_context,
			&constant_motive_claim
		) != 0 || prototype_term_app(
			&term_db,
			constant_motive,
			constant_function_argument,
			&matched_constant_classifier
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			constant_function_context,
			matched_constant_classifier,
			universe,
			constant_motive_claim,
			constant_function_argument_claim,
			&matched_constant_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			constant_function_context,
			matched_constant_classifier,
			universe,
			matched_constant_classifier_claim,
			&matched_constant_classifier_is_type_claim
		) != 0 || prototype_term_match(
			&term_db,
			constant_function_argument,
			constant_match_cases,
			2,
			&matched_constant_body
		) != 0 || prototype_judgement_add_match_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&operation_graph,
			constant_function_context,
			matched_constant_body,
			matched_constant_classifier,
			matched_constant_classifier_claim,
			constant_branch_claims,
			2,
			&matched_constant_body_claim
		) != 0) {
		return 322;
	}
	uint32_t matched_constant_lambda;
	uint32_t matched_constant_lambda_claim;
	uint32_t matched_constant_function_value;
	uint32_t matched_constant_function_value_claim;
	if (prototype_term_lambda(
			&term_db,
			constant_function_binding,
			matched_constant_body,
			&matched_constant_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			matched_constant_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			matched_constant_body_claim,
			&matched_constant_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db,
			matched_constant_lambda,
			&matched_constant_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			matched_constant_function_value,
			thunk_bool_function_type,
			matched_constant_lambda_claim,
			&matched_constant_function_value_claim
		) != 0) {
		return 323;
	}
	if (prototype_judgement_classifier_conversion(
			&term_db,
			&type_db,
			constant_function_value,
			matched_constant_function_value
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 324;
	}
	uint32_t constant_function_identity_family_claim;
	if (prototype_hott_instantiate_object_identity_family(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			matched_constant_function_value_claim,
			&constant_function_identity_family_claim
		) != 0) {
		return 325;
	}
	uint32_t constant_function_degeneracy_family_claim;
	uint32_t constant_function_degeneracy_witness;
	uint32_t constant_function_degeneracy_witness_claim;
	uint32_t checked_constant_function_family_claim;
	uint32_t checked_constant_function_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			&constant_function_degeneracy_family_claim,
			&constant_function_degeneracy_witness,
			&constant_function_degeneracy_witness_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			matched_constant_function_value_claim,
			constant_function_degeneracy_witness_claim,
			&checked_constant_function_family_claim,
			&checked_constant_function_witness_claim
		) != 0 || checked_constant_function_family_claim !=
			constant_function_identity_family_claim) {
		return 326;
	}
	uint32_t observed_false_claim;
	uint32_t observed_false_return;
	uint32_t observed_false_return_claim;
	uint32_t observed_false_lambda;
	uint32_t observed_false_lambda_claim;
	uint32_t observed_false_function;
	uint32_t observed_false_function_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			false_claim,
			constant_function_projection,
			&observed_false_claim
		) != 0 || prototype_term_return(
			&term_db, bool_false, &observed_false_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			constant_function_context,
			observed_false_return,
			pure_bool_computation_type,
			observed_false_claim,
			&observed_false_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			constant_function_binding,
			observed_false_return,
			&observed_false_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			observed_false_lambda,
			bool_function_computation_type,
			constant_function_argument_claim,
			observed_false_return_claim,
			&observed_false_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, observed_false_lambda, &observed_false_function
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			observed_false_function,
			thunk_bool_function_type,
			observed_false_lambda_claim,
			&observed_false_function_claim
		) != 0 || prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			observed_false_function_claim,
			constant_function_degeneracy_witness_claim,
			&checked_constant_function_family_claim,
			&checked_constant_function_witness_claim
		) == 0) {
		return 327;
	}
	(void)constant_function_context_certificate;
	(void)constant_function_degeneracy_family_claim;
	(void)constant_function_degeneracy_witness;
	uint32_t pure_function_value_computation_type;
	uint32_t thunk_return_function_type;
	uint32_t thunk_return_function_has_type;
	uint32_t thunk_return_function_is_type;
	uint32_t returned_function_value;
	uint32_t returned_function_value_claim;
	uint32_t thunked_returned_function;
	uint32_t thunked_returned_function_claim;
	if (prototype_term_computation_type(
			&term_db,
			empty_effect_row,
			thunk_bool_function_type,
			&pure_function_value_computation_type
		) != 0 || prototype_term_thunk_type(
			&term_db,
			pure_function_value_computation_type,
			&thunk_return_function_type
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			thunk_return_function_type,
			universe,
			&thunk_return_function_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			thunk_return_function_type,
			universe,
			thunk_return_function_has_type,
			&thunk_return_function_is_type
		) != 0 || prototype_term_return(
			&term_db, identity_function_value, &returned_function_value
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			returned_function_value,
			pure_function_value_computation_type,
			identity_function_value_claim,
			&returned_function_value_claim
		) != 0 || prototype_term_thunk(
			&term_db, returned_function_value, &thunked_returned_function
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			thunked_returned_function,
			thunk_return_function_type,
			returned_function_value_claim,
			&thunked_returned_function_claim
		) != 0) {
		return 267;
	}
	struct prototype_hott_action_request thunk_return_function_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = thunk_return_function_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t thunk_return_function_identity_request_id;
	uint32_t thunk_return_function_identity_result_id;
	uint32_t thunk_return_function_family_claim;
	uint32_t thunk_return_function_witness;
	uint32_t thunk_return_function_witness_claim;
	int thunk_return_function_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			thunk_return_function_identity_request,
			&thunk_return_function_identity_request_id
		);
	int thunk_return_function_execution_status =
		thunk_return_function_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_return_function_identity_request_id,
			&thunk_return_function_identity_result_id
		) : -1;
	if (thunk_return_function_request_status != 0 ||
		thunk_return_function_execution_status != 0 || action_results[
			thunk_return_function_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[thunk_return_function_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
		uint32_t generated_nested_identity = PROTOTYPE_INVALID_ID;
		int generated_nested_status =
			prototype_type_declaration_find_generated_identity(
				&type_db,
				thunk_return_function_type,
				empty,
				&generated_nested_identity
			);
		int generated_nested_valid = generated_nested_status == 0 ?
			prototype_type_declaration_validate_generated_identity(
				&term_db,
				&type_db,
				&context_db,
				thunk_return_function_type,
				generated_nested_identity,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN
			) : -1;
		fprintf(
			stderr,
			"nested Pi carrier identity computation failed intern=%d execute=%d "
			"request=%u result=%u "
			"state=%d result_request=%u certificate=%u rule=%d generated=%u "
			"generated_status=%d generated_valid=%d\n",
			thunk_return_function_request_status,
			thunk_return_function_execution_status,
			thunk_return_function_identity_request_id,
			thunk_return_function_identity_result_id,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].outcome.state :
				-1,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].request_id :
				PROTOTYPE_INVALID_ID,
			thunk_return_function_identity_result_id < action_db.result_count ?
				action_results[thunk_return_function_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			thunk_return_function_identity_result_id < action_db.result_count &&
				action_results[thunk_return_function_identity_result_id].certificate_id <
					action_db.certificate_count ? action_db.certificates[
						action_results[
							thunk_return_function_identity_result_id
						].certificate_id
					].data.identity_type.computation_rule : -1,
			generated_nested_identity,
			generated_nested_status,
			generated_nested_valid
		);
		return 268;
	}
	int thunk_return_function_degeneracy_status =
		prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			thunk_return_function_identity_result_id,
			thunked_returned_function_claim,
			&thunk_return_function_family_claim,
			&thunk_return_function_witness,
			&thunk_return_function_witness_claim
		);
	if (thunk_return_function_degeneracy_status != 0 ||
		prototype_judgement_claim_proposition(
			&judgement, thunk_return_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, thunk_return_function_family_claim
		)->subject) {
		fprintf(
			stderr,
			"nested Pi carrier degeneracy failed status=%d family=%u witness=%u "
			"claim=%u\n",
			thunk_return_function_degeneracy_status,
			thunk_return_function_family_claim,
			thunk_return_function_witness,
			thunk_return_function_witness_claim
		);
		return 268;
	}
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		fprintf(stderr, "nested Pi carrier replay failed\n");
		return 268;
	}
	uint32_t thunk_bool_function_is_type_in_endpoint;
	struct prototype_hott_action_request constant_pi_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_endpoint_bridge
		}
	};
	uint32_t constant_pi_identity_computation_id;
	uint32_t constant_pi_identity_result_id;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_function_is_type,
			first_endpoint_to_empty,
			&thunk_bool_function_is_type_in_endpoint
		) != 0) {
		return 256;
	}
	constant_pi_identity_computation.key.identity_type.source_claim_id =
		thunk_bool_function_is_type_in_endpoint;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_pi_identity_computation,
			&constant_pi_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_pi_identity_computation_id,
			&constant_pi_identity_result_id
		) != 0 || action_results[
			constant_pi_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			constant_pi_identity_result_id
		].certificate_id >= action_db.certificate_count || action_db.certificates[
			action_results[constant_pi_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT ||
		action_db.certificates[action_results[
			constant_pi_identity_result_id
		].certificate_id].data.identity_type.generated_type_declaration_id !=
			PROTOTYPE_INVALID_ID || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 257;
	}
	uint32_t contextual_function_binding = prototype_term_new_binding(&term_db);
	uint32_t contextual_function_body_context;
	uint32_t contextual_function_body_context_certificate;
	uint32_t contextual_function_argument;
	uint32_t contextual_function_argument_claim;
	uint32_t contextual_function_outer_value;
	uint32_t contextual_function_outer_value_claim;
	uint32_t contextual_function_return;
	uint32_t contextual_function_return_claim;
	uint32_t contextual_function_lambda;
	uint32_t contextual_function_lambda_claim;
	uint32_t contextual_function_value;
	uint32_t contextual_function_value_claim;
	if (contextual_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			bool_identity_endpoint_path[0],
			contextual_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&contextual_function_body_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			contextual_function_body_context,
			bool_is_type_in_first_endpoint,
			&contextual_function_body_context_certificate
		) != 0 || prototype_term_var(
			&term_db, contextual_function_binding, &contextual_function_argument
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			contextual_function_body_context,
			contextual_function_binding,
			bool_view,
			&contextual_function_argument_claim
		) != 0 || prototype_term_var(
			&term_db, endpoint_bool_binding, &contextual_function_outer_value
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			contextual_function_body_context,
			endpoint_bool_binding,
			bool_view,
			&contextual_function_outer_value_claim
		) != 0 || prototype_term_return(
			&term_db,
			contextual_function_outer_value,
			&contextual_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			contextual_function_body_context,
			contextual_function_return,
			pure_bool_computation_type,
			contextual_function_outer_value_claim,
			&contextual_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			contextual_function_binding,
			contextual_function_return,
			&contextual_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_identity_endpoint_path[0],
			contextual_function_lambda,
			bool_function_computation_type,
			contextual_function_argument_claim,
			contextual_function_return_claim,
			&contextual_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, contextual_function_lambda, &contextual_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_identity_endpoint_path[0],
			contextual_function_value,
			thunk_bool_function_type,
			contextual_function_lambda_claim,
			&contextual_function_value_claim
		) != 0) {
		return 262;
	}
	uint32_t contextual_function_family_claim;
	uint32_t contextual_function_witness;
	uint32_t contextual_function_witness_claim;
	int contextual_function_action_status =
		prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_pi_identity_result_id,
			contextual_function_value_claim,
			&contextual_function_family_claim,
			&contextual_function_witness,
			&contextual_function_witness_claim
		);
	if (contextual_function_action_status != 0 ||
		contextual_function_witness >= term_db.term_count || term_db.terms[
			contextual_function_witness
		].tag != PROTOTYPE_TERM_THUNK || prototype_judgement_claim_proposition(
			&judgement, contextual_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, contextual_function_family_claim
		)->subject) {
		fprintf(
			stderr,
			"contextual Pi action failed status=%d family=%u witness=%u claim=%u\n",
			contextual_function_action_status,
			contextual_function_family_claim,
			contextual_function_witness,
			contextual_function_witness_claim
		);
		return 263;
	}
	(void)contextual_function_body_context_certificate;
	uint32_t pi_variable_binding = prototype_term_new_binding(&term_db);
	uint32_t pi_variable_context;
	uint32_t pi_variable_context_certificate;
	uint32_t pi_variable_bridge;
	if (pi_variable_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			pi_variable_binding,
			thunk_bool_function_type,
			PROTOTYPE_INVALID_ID,
			&pi_variable_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			pi_variable_context,
			thunk_bool_function_is_type,
			&pi_variable_context_certificate
		) != 0 || prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			pi_variable_context,
			bool_function_identity_computation_id,
			&pi_variable_bridge
		) != 0) {
		return 258;
	}
	uint32_t pi_variable_projection;
	uint32_t pi_type_in_variable_context;
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			pi_variable_context,
			empty,
			&pi_variable_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			thunk_bool_function_is_type,
			pi_variable_projection,
			&pi_type_in_variable_context
		) != 0) {
		return 259;
	}
	struct prototype_hott_action_request pi_variable_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = pi_type_in_variable_context,
			.source_bridge_id = pi_variable_bridge
		}
	};
	uint32_t pi_variable_identity_computation_id;
	uint32_t pi_variable_identity_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			pi_variable_identity_computation,
			&pi_variable_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			pi_variable_identity_computation_id,
			&pi_variable_identity_result_id
		) != 0 || action_results[pi_variable_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[pi_variable_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) {
		return 260;
	}
	uint32_t pi_variable_claim;
	uint32_t pi_variable_family_claim;
	uint32_t pi_variable_witness;
	uint32_t pi_variable_witness_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			pi_variable_context,
			pi_variable_binding,
			thunk_bool_function_type,
			&pi_variable_claim
		) != 0 || prototype_hott_construct_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			pi_variable_identity_result_id,
			pi_variable_claim,
			&pi_variable_family_claim,
			&pi_variable_witness,
			&pi_variable_witness_claim
		) != 0 || pi_variable_witness !=
			prototype_judgement_claim_proposition(
				&judgement,
				bridge_certificates[pi_variable_bridge].fiber_witness_claim_id
			)->subject || prototype_judgement_claim_proposition(
			&judgement, pi_variable_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, pi_variable_family_claim
		)->subject || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 261;
	}
	uint32_t applied_function_bool_type_claim;
	uint32_t applied_function_binding = prototype_term_new_binding(&term_db);
	uint32_t applied_function_context;
	uint32_t applied_function_context_certificate;
	uint32_t applied_function_variable;
	uint32_t applied_function_variable_claim;
	uint32_t applied_argument_variable;
	uint32_t applied_argument_claim;
	uint32_t applied_force;
	uint32_t applied_force_claim;
	uint32_t applied_body;
	uint32_t applied_body_claim;
	uint32_t applied_lambda;
	uint32_t applied_lambda_claim;
	uint32_t applied_value;
	uint32_t applied_value_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			pi_variable_projection,
			&applied_function_bool_type_claim
		) != 0 || applied_function_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			pi_variable_context,
			applied_function_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&applied_function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			applied_function_context,
			applied_function_bool_type_claim,
			&applied_function_context_certificate
		) != 0 || prototype_term_var(
			&term_db, pi_variable_binding, &applied_function_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			applied_function_context,
			pi_variable_binding,
			thunk_bool_function_type,
			&applied_function_variable_claim
		) != 0 || prototype_term_var(
			&term_db, applied_function_binding, &applied_argument_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			applied_function_context,
			applied_function_binding,
			bool_view,
			&applied_argument_claim
		) != 0 || prototype_term_force(
			&term_db, applied_function_variable, &applied_force
		) != 0 || prototype_judgement_add_force_claim(
			&judgement,
			&term_db,
			applied_function_context,
			applied_force,
			bool_function_computation_type,
			applied_function_variable_claim,
			&applied_force_claim
		) != 0 || prototype_term_app(
			&term_db, applied_force, applied_argument_variable, &applied_body
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			applied_function_context,
			applied_body,
			pure_bool_computation_type,
			applied_force_claim,
			applied_argument_claim,
			&applied_body_claim
		) != 0 || prototype_term_lambda(
			&term_db, applied_function_binding, applied_body, &applied_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pi_variable_context,
			applied_lambda,
			bool_function_computation_type,
			applied_argument_claim,
			applied_body_claim,
			&applied_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, applied_lambda, &applied_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			pi_variable_context,
			applied_value,
			thunk_bool_function_type,
			applied_lambda_claim,
			&applied_value_claim
		) != 0) {
		return 269;
	}
	uint32_t applied_family_claim;
	uint32_t applied_witness;
	uint32_t applied_witness_claim;
	size_t applied_derivation_start = judgement.derivation_count;
	int applied_action_status = prototype_hott_construct_object_term_action(
		&action_db,
		&kernel_builder,
		&bridge_db,
		pi_variable_identity_result_id,
		applied_value_claim,
		&applied_family_claim,
		&applied_witness,
		&applied_witness_claim
	);
	int found_applied_fold = 0;
	for (size_t i = applied_derivation_start; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM) {
			found_applied_fold = 1;
			break;
		}
	}
	if (applied_action_status != 0 || !found_applied_fold ||
		applied_witness >= term_db.term_count ||
		term_db.terms[applied_witness].tag != PROTOTYPE_TERM_THUNK ||
		prototype_judgement_claim_proposition(
			&judgement, applied_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, applied_family_claim
		)->subject || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		fprintf(
			stderr,
			"applied Pi action failed status=%d fold=%d family=%u witness=%u "
			"claim=%u\n",
			applied_action_status,
			found_applied_fold,
			applied_family_claim,
			applied_witness,
			applied_witness_claim
		);
		return 269;
	}
	(void)applied_function_context_certificate;
	(void)pi_variable_context_certificate;
	uint32_t saved_pi_identity_term =
		bool_function_identity_certificate->data.identity_type.identity_type_term_id;
	bool_function_identity_certificate->data.identity_type.identity_type_term_id =
		thunk_bool_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 187;
	}
	bool_function_identity_certificate->data.identity_type.identity_type_term_id =
		saved_pi_identity_term;
	size_t pi_replay_term_count = term_db.term_count;
	uint32_t pi_replay_next_binding = term_db.next_binding_id;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0 || term_db.term_count != pi_replay_term_count ||
		term_db.next_binding_id != pi_replay_next_binding) {
		return 188;
	}
	uint32_t pi_degeneracy_family;
	if (prototype_term_graph_substitute_bound_var(
			&term_db,
			&type_db,
			bool_function_identity_certificate->data.identity_type.
				identity_type_term_id,
			bool_function_identity_certificate->data.identity_type.
				left_endpoint_binding_id,
			identity_function_value,
			&pi_degeneracy_family
		) != 0 || prototype_term_graph_substitute_bound_var(
			&term_db,
			&type_db,
			pi_degeneracy_family,
			bool_function_identity_certificate->data.identity_type.
				right_endpoint_binding_id,
			identity_function_value,
			&pi_degeneracy_family
		) != 0) {
		return 225;
	}
	uint32_t pi_degeneracy_family_claim = PROTOTYPE_INVALID_ID;
	uint32_t pi_degeneracy_witness = PROTOTYPE_INVALID_ID;
	uint32_t pi_degeneracy_witness_claim = PROTOTYPE_INVALID_ID;
	int pi_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			identity_function_value_claim,
			&pi_degeneracy_family_claim,
			&pi_degeneracy_witness,
			&pi_degeneracy_witness_claim
		);
	if (pi_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, pi_degeneracy_family_claim
		)->subject != pi_degeneracy_family ||
		prototype_judgement_claim_proposition(
			&judgement, pi_degeneracy_witness_claim
		)->classifier != pi_degeneracy_family ||
		pi_degeneracy_witness >= term_db.term_count ||
		term_db.terms[pi_degeneracy_witness].tag != PROTOTYPE_TERM_THUNK) {
		fprintf(
			stderr,
			"pi degeneracy failed status=%d family=%u witness=%u claim=%u\n",
			pi_degeneracy_status,
			pi_degeneracy_family_claim,
			pi_degeneracy_witness,
			pi_degeneracy_witness_claim
		);
		return 226;
	}
	uint32_t pi_ap_left;
	uint32_t pi_ap_right;
	uint32_t pi_ap_identity;
	if (apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_degeneracy_witness,
			bool_false,
			&pi_ap_left
		) != 0 || apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_ap_left,
			bool_false,
			&pi_ap_right
		) != 0 || apply_pure_thunk_function(
			&term_db,
			&type_db,
			pi_ap_right,
			false_identity_witness,
			&pi_ap_identity
		) != 0 || pi_ap_identity != thunk_degeneracy_witness) {
		fprintf(
			stderr,
			"Pi ap/refl law failed left=%u right=%u result=%u expected=%u\n",
			pi_ap_left,
			pi_ap_right,
			pi_ap_identity,
			thunk_degeneracy_witness
		);
		return 271;
	}
	uint32_t composed_identity_binding = prototype_term_new_binding(&term_db);
	uint32_t composed_identity_context;
	uint32_t composed_identity_context_certificate;
	uint32_t composed_identity_projection;
	uint32_t composed_identity_function_claim;
	uint32_t composed_identity_argument;
	uint32_t composed_identity_argument_claim;
	uint32_t composed_identity_force;
	uint32_t composed_identity_force_claim;
	uint32_t composed_identity_app;
	uint32_t composed_identity_app_claim;
	uint32_t composed_identity_lambda;
	uint32_t composed_identity_lambda_claim;
	uint32_t composed_identity_value;
	uint32_t composed_identity_value_claim;
	uint32_t composed_identity_family_claim;
	uint32_t composed_identity_witness;
	uint32_t composed_identity_witness_claim;
	if (composed_identity_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			composed_identity_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&composed_identity_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			composed_identity_context,
			bool_is_type,
			&composed_identity_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			composed_identity_context,
			empty,
			&composed_identity_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			identity_function_value_claim,
			composed_identity_projection,
			&composed_identity_function_claim
		) != 0 || prototype_term_var(
			&term_db, composed_identity_binding, &composed_identity_argument
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			composed_identity_context,
			composed_identity_binding,
			bool_view,
			&composed_identity_argument_claim
		) != 0 || prototype_term_force(
			&term_db, identity_function_value, &composed_identity_force
		) != 0 || prototype_judgement_add_force_claim(
			&judgement,
			&term_db,
			composed_identity_context,
			composed_identity_force,
			bool_function_computation_type,
			composed_identity_function_claim,
			&composed_identity_force_claim
		) != 0 || prototype_term_app(
			&term_db,
			composed_identity_force,
			composed_identity_argument,
			&composed_identity_app
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			composed_identity_context,
			composed_identity_app,
			pure_bool_computation_type,
			composed_identity_force_claim,
			composed_identity_argument_claim,
			&composed_identity_app_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			composed_identity_binding,
			composed_identity_app,
			&composed_identity_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			composed_identity_lambda,
			bool_function_computation_type,
			composed_identity_argument_claim,
			composed_identity_app_claim,
			&composed_identity_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db, composed_identity_lambda, &composed_identity_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			composed_identity_value,
			thunk_bool_function_type,
			composed_identity_lambda_claim,
			&composed_identity_value_claim
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			composed_identity_value_claim,
			&composed_identity_family_claim,
			&composed_identity_witness,
			&composed_identity_witness_claim
		) != 0) {
		return 272;
	}
	(void)composed_identity_context_certificate;
	uint32_t pi_identity_root_id;
	uint32_t concrete_pi_identity_root_id;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_function_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&pi_identity_root_id
		) != 0 || pi_identity_root_id != 4 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_function_is_type,
			pi_degeneracy_family_claim,
			pi_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&concrete_pi_identity_root_id
		) != 0 || concrete_pi_identity_root_id != 5 ||
		prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 229;
	}
	uint32_t artifact_pi_family =
		bool_function_identity_certificate->data.identity_type.identity_type_term_id;
	uint32_t artifact_outer_pi = artifact_pi_family < term_db.term_count &&
		term_db.terms[artifact_pi_family].tag == PROTOTYPE_TERM_THUNK_TYPE ?
		term_db.terms[artifact_pi_family].as.thunk_type.computation :
		PROTOTYPE_INVALID_ID;
	if (artifact_outer_pi >= term_db.term_count ||
		term_db.terms[artifact_outer_pi].tag != PROTOTYPE_TERM_PI) {
		return 232;
	}
	uint32_t artifact_saved_pi_domain =
		term_db.terms[artifact_outer_pi].as.pi.domain;
	struct prototype_term* forged_pi_terms = malloc(
		term_db.term_capacity * sizeof(*forged_pi_terms)
	);
	if (!forged_pi_terms) {
		return 233;
	}
	memcpy(
		forged_pi_terms,
		term_db.terms,
		term_db.term_capacity * sizeof(*forged_pi_terms)
	);
	struct prototype_term_db forged_pi_term_db = term_db;
	forged_pi_term_db.terms = forged_pi_terms;
	forged_pi_term_db.terms[artifact_outer_pi].as.pi.domain = box_view;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&forged_pi_term_db,
			&type_db,
			&context_db,
			&judgement
		) == 0) {
		free(forged_pi_terms);
		return 233;
	}
	free(forged_pi_terms);
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) != 0) {
		return 234;
	}
	(void)artifact_saved_pi_domain;
	uint32_t constant_family_function_projection;
	uint32_t constant_function_false_claim;
	uint32_t constant_function_return;
	uint32_t constant_function_return_claim;
	uint32_t constant_family_function_lambda;
	uint32_t constant_family_function_lambda_claim;
	uint32_t constant_family_function_value;
	uint32_t constant_family_function_value_claim;
	uint32_t constant_function_family_claim;
	uint32_t constant_function_witness;
	uint32_t constant_function_witness_claim;
	uint32_t canonical_false_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_false,
			bool_view,
			&canonical_false_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			identity_function_context,
			empty,
			&constant_family_function_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			canonical_false_claim,
			constant_family_function_projection,
			&constant_function_false_claim
		) != 0 || prototype_term_return(
			&term_db, bool_false, &constant_function_return
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			identity_function_context,
			constant_function_return,
			pure_bool_computation_type,
			constant_function_false_claim,
			&constant_function_return_claim
		) != 0 || prototype_term_lambda(
			&term_db,
			identity_function_binding,
			constant_function_return,
			&constant_family_function_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			constant_family_function_lambda,
			bool_function_computation_type,
			identity_function_var_claim,
			constant_function_return_claim,
			&constant_family_function_lambda_claim
		) != 0 || prototype_term_thunk(
			&term_db,
			constant_family_function_lambda,
			&constant_family_function_value
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			constant_family_function_value,
			thunk_bool_function_type,
			constant_family_function_lambda_claim,
			&constant_family_function_value_claim
		) != 0) {
		return 227;
	}
	int constant_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_family_function_value_claim,
			&constant_function_family_claim,
			&constant_function_witness,
			&constant_function_witness_claim
		);
	if (constant_degeneracy_status != 0 || prototype_judgement_claim_proposition(
			&judgement, constant_function_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, constant_function_family_claim
		)->subject || constant_function_witness >= term_db.term_count ||
		term_db.terms[constant_function_witness].tag != PROTOTYPE_TERM_THUNK) {
		return 227;
	}
	uint32_t extensional_identity_family_claim;
	uint32_t extensional_identity_witness;
	uint32_t extensional_identity_witness_claim;
	int extensional_identity_status = check_bool_function_extensional_identity(
		&term_db,
		&type_db,
		&context_db,
		&substitution_db,
		&judgement,
		&operation_graph,
		empty,
		universe,
		bool_view,
		bool_false,
		bool_true,
		generated_bool_identity_type,
		generated_thunk_bool_identity_type,
		thunk_bool_identity_certificate->data.identity_type.
			backing_type_former_term_id,
		thunk_bool_function_type,
		&bool_function_identity_certificate->data.identity_type,
		&extensional_identity_family_claim,
		&extensional_identity_witness,
		&extensional_identity_witness_claim
	);
	uint32_t extensional_identity_root;
	int extensional_root_status = extensional_identity_status == 0 &&
		extensional_identity_witness < term_db.term_count ?
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			thunk_bool_function_is_type,
			extensional_identity_family_claim,
			extensional_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&extensional_identity_root
		) : -1;
	int extensional_root_validation_status = extensional_root_status == 0 ?
		prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement
		) : -1;
	if (extensional_identity_status != 0 ||
		extensional_identity_witness >= term_db.term_count ||
		extensional_root_status != 0 || extensional_root_validation_status != 0) {
		fprintf(stderr,
			"HOTT extensional Identity root failed identity-status=%d "
			"witness=%u term-count=%zu root-status=%d validation-status=%d\n",
			extensional_identity_status,
			extensional_identity_witness,
			term_db.term_count,
			extensional_root_status,
			extensional_root_validation_status);
		return 189;
	}
	(void)extensional_identity_root;
	struct prototype_hott_action_request box_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t box_identity_computation_id;
	uint32_t box_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			box_identity_computation,
			&box_identity_computation_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_id,
			&box_identity_computation_result_id
		) != 0 || action_results[box_identity_computation_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[box_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 170;
	}
	const struct prototype_hott_action_certificate* box_identity_certificate =
		&action_db.certificates[
			action_results[box_identity_computation_result_id].certificate_id
		];
	uint32_t generated_box_identity_type =
		box_identity_certificate->data.identity_type.
			generated_type_declaration_id;
	const struct prototype_type_declaration* generated_box_identity_declaration =
		&type_db.type_declarations[generated_box_identity_type];
	const struct prototype_type_constructor_declaration*
		generated_box_identity_constructor = &type_db.constructor_declarations[
			generated_box_identity_declaration->first_constructor
		];
	const struct prototype_context* generated_box_identity_fields =
		prototype_context_get(
			&context_db, generated_box_identity_constructor->field_context
		);
	if (box_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		generated_box_identity_declaration->constructor_count != 1 ||
		!generated_box_identity_fields ||
		generated_box_identity_fields->depth != 3 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 171;
	}
	struct prototype_hott_action_request dependent_box_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = dependent_box_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t dependent_box_identity_request_id;
	uint32_t dependent_box_identity_result_id;
	int dependent_box_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			dependent_box_identity_computation,
			&dependent_box_identity_request_id
		);
	int dependent_box_execution_status = dependent_box_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			dependent_box_identity_request_id,
			&dependent_box_identity_result_id
		) : -1;
	if (dependent_box_request_status != 0 || dependent_box_execution_status != 0 ||
		action_results[
			dependent_box_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			dependent_box_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		fprintf(
			stderr,
			"dependent Box identity failed request_status=%d execute_status=%d "
			"request=%u result=%u state=%d residual=%d certificate=%u count=%zu\n",
			dependent_box_request_status,
			dependent_box_execution_status,
			dependent_box_request_status == 0 ? dependent_box_identity_request_id :
				PROTOTYPE_INVALID_ID,
			dependent_box_execution_status == 0 ? dependent_box_identity_result_id :
				PROTOTYPE_INVALID_ID,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].outcome.state : -1,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].outcome.residual_reason : -1,
			dependent_box_execution_status == 0 ? action_results[
				dependent_box_identity_result_id
			].certificate_id : PROTOTYPE_INVALID_ID,
			action_db.certificate_count
		);
		return 309;
	}
	const struct prototype_hott_action_certificate* dependent_box_identity =
		&action_db.certificates[action_results[
			dependent_box_identity_result_id
		].certificate_id];
	const struct prototype_type_declaration* generated_dependent_box_identity =
		dependent_box_identity->data.identity_type.generated_type_declaration_id <
			type_db.type_count ? &type_db.type_declarations[
				dependent_box_identity->data.identity_type.
					generated_type_declaration_id
			] : NULL;
	const struct prototype_type_constructor_declaration*
		generated_dependent_box_constructor = generated_dependent_box_identity &&
		generated_dependent_box_identity->first_constructor <
			type_db.constructor_count ? &type_db.constructor_declarations[
				generated_dependent_box_identity->first_constructor
			] : NULL;
	const struct prototype_context* generated_dependent_box_fields =
		generated_dependent_box_constructor ? prototype_context_get(
			&context_db, generated_dependent_box_constructor->field_context
		) : NULL;
	uint32_t dependent_field_bridge = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < bridge_db.bridge_count; ++i) {
		if (bridge_db.bridges[i].source_context_id ==
				dependent_box_constructor->field_context &&
			bridge_db.certificates[i].semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) {
			dependent_field_bridge = i;
			break;
		}
	}
	const struct prototype_hott_bridge_certificate* dependent_bridge_certificate =
		dependent_field_bridge < bridge_db.certificate_count ?
		&bridge_db.certificates[dependent_field_bridge] : NULL;
	const struct prototype_hott_action_certificate* dependent_fiber_certificate =
		dependent_bridge_certificate && dependent_bridge_certificate->
			fiber_action_certificate_id < action_db.certificate_count ?
		&action_db.certificates[
			dependent_bridge_certificate->fiber_action_certificate_id
		] : NULL;
	if (!generated_dependent_box_identity ||
		!generated_dependent_box_constructor ||
		!generated_dependent_box_fields ||
		generated_dependent_box_identity->constructor_count != 1 ||
		generated_dependent_box_fields->depth != 6 ||
		!dependent_fiber_certificate || dependent_fiber_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		dependent_fiber_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 310;
	}
	uint32_t dependent_box_false;
	uint32_t dependent_box_false_claim;
	uint32_t dependent_box_family_claim;
	uint32_t dependent_box_witness;
	uint32_t dependent_box_witness_claim;
	int dependent_box_constructor_status = prototype_term_constructor(
			&term_db, dependent_box_view, 0, &dependent_box_false
		);
	int dependent_box_app0_status = dependent_box_constructor_status == 0 ?
		prototype_term_app(
			&term_db, dependent_box_false, bool_false, &dependent_box_false
		) : -1;
	int dependent_box_app1_status = dependent_box_app0_status == 0 ?
		prototype_term_app(
			&term_db, dependent_box_false, bool_false, &dependent_box_false
		) : -1;
	int dependent_box_claim_status = dependent_box_app1_status == 0 ?
		prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			dependent_box_false,
			dependent_box_view,
			(uint32_t[]) {
				bool_false_constructor_claim,
				bool_false_constructor_claim
			},
			2,
			&dependent_box_false_claim
		) : -1;
	int dependent_box_degeneracy_status = dependent_box_claim_status == 0 ?
		prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			dependent_box_identity_result_id,
			dependent_box_false_claim,
			&dependent_box_family_claim,
			&dependent_box_witness,
			&dependent_box_witness_claim
		) : -1;
	if (dependent_box_constructor_status != 0 || dependent_box_app0_status != 0 ||
		dependent_box_app1_status != 0 || dependent_box_claim_status != 0 ||
		dependent_box_degeneracy_status != 0) {
		int dependent_builder_status = prototype_kernel_builder_validate_stores(
			&kernel_builder
		);
		int dependent_action_status = dependent_builder_status == 0 ?
			prototype_hott_action_db_validate(
				&action_db, &kernel_view, &bridge_db
			) : -1;
		fprintf(stderr,
			"HOTT dependent box degeneracy failed constructor=%d app0=%d "
			"app1=%d claim=%d degeneracy=%d builder=%d action=%d\n",
			dependent_box_constructor_status, dependent_box_app0_status,
			dependent_box_app1_status, dependent_box_claim_status,
			dependent_box_degeneracy_status, dependent_builder_status,
			dependent_action_status);
		return 311;
	}
	uint32_t dependent_witness_head;
	uint32_t dependent_witness_owner;
	uint32_t dependent_witness_ordinal;
	uint32_t dependent_witness_arguments[6];
	uint32_t dependent_witness_argument_count;
	const struct prototype_judgement_proposition* dependent_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_family_claim
		);
	const struct prototype_judgement_proposition* dependent_witness_claim =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_witness_claim
		);
	if (!dependent_identity_family || !dependent_witness_claim ||
		dependent_witness_claim->classifier != dependent_identity_family->subject ||
		prototype_term_constructor_spine_info(
			&term_db,
			dependent_box_witness,
			&dependent_witness_head,
			&dependent_witness_owner,
			&dependent_witness_ordinal,
			dependent_witness_arguments,
			6,
			&dependent_witness_argument_count
		) != 0 || dependent_witness_ordinal != 0 ||
		dependent_witness_argument_count != 6 ||
		dependent_witness_arguments[0] != bool_false ||
		dependent_witness_arguments[1] != bool_false ||
		dependent_witness_arguments[2] != false_identity_witness ||
		dependent_witness_arguments[3] != bool_false ||
		dependent_witness_arguments[4] != bool_false ||
		dependent_witness_arguments[5] != false_identity_witness) {
		return 312;
	}
	uint32_t box_false;
	uint32_t box_false_right;
	uint32_t box_identity_witness;
	uint32_t box_identity_type_arguments[2];
	uint32_t box_identity_type;
	if (prototype_term_constructor(
			&term_db, box_view, 0, &box_false
		) != 0 || prototype_term_app(
			&term_db, box_false, bool_false, &box_false
		) != 0 || prototype_term_constructor(
			&term_db, box_view, 0, &box_false_right
		) != 0 || prototype_term_app(
			&term_db, box_false_right, bool_false, &box_false_right
		) != 0 || prototype_term_constructor(
			&term_db,
			box_identity_certificate->data.identity_type.backing_type_former_term_id,
			0,
			&box_identity_witness
		) != 0 || prototype_term_app(
			&term_db, box_identity_witness, bool_false, &box_identity_witness
		) != 0 || prototype_term_app(
			&term_db, box_identity_witness, bool_false, &box_identity_witness
		) != 0 || prototype_term_app(
			&term_db,
			box_identity_witness,
			false_identity_witness,
			&box_identity_witness
		) != 0) {
		return 174;
	}
	box_identity_type_arguments[0] = box_false;
	box_identity_type_arguments[1] = box_false_right;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_box_identity_type,
			box_identity_type_arguments,
			2,
			&box_identity_type
		) != 0) {
		return 174;
	}
	uint32_t box_false_claim;
	uint32_t box_degeneracy_family_claim;
	uint32_t box_degeneracy_witness;
	uint32_t box_degeneracy_witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			box_false,
			box_view,
			(uint32_t[]) { bool_false_constructor_claim },
			1,
			&box_false_claim
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_result_id,
			box_false_claim,
			&box_degeneracy_family_claim,
			&box_degeneracy_witness,
			&box_degeneracy_witness_claim
		) != 0 || box_degeneracy_witness != box_identity_witness ||
		prototype_judgement_claim_proposition(
			&judgement, box_degeneracy_family_claim
		)->subject != box_identity_type || prototype_judgement_claim_proposition(
			&judgement, box_degeneracy_witness_claim
		)->classifier != box_identity_type) {
		return 221;
	}
	uint32_t box_endpoint_path[2];
	uint32_t box_endpoint_count;
	uint32_t first_box_endpoint_bridge;
	uint32_t first_box_endpoint_to_empty;
	uint32_t box_is_type_in_first_endpoint;
	if (prototype_context_extension_path(
			&context_db,
			empty,
			box_identity_certificate->data.identity_type.endpoint_context_id,
			box_endpoint_path,
			2,
			&box_endpoint_count
		) != 0 || box_endpoint_count != 2 ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			box_endpoint_path[0],
			box_identity_computation_id,
			&first_box_endpoint_bridge
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			box_endpoint_path[0],
			empty,
			&first_box_endpoint_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			box_is_type,
			first_box_endpoint_to_empty,
			&box_is_type_in_first_endpoint
		) != 0) {
		return 247;
	}
	struct prototype_hott_action_request constant_box_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_is_type_in_first_endpoint,
			.source_bridge_id = first_box_endpoint_bridge
		}
	};
	uint32_t constant_box_identity_id;
	uint32_t constant_box_identity_result_id;
	uint32_t full_box_endpoint_bridge;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			constant_box_identity,
			&constant_box_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			constant_box_identity_id,
			&constant_box_identity_result_id
		) != 0 || action_results[constant_box_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			box_endpoint_path[1],
			constant_box_identity_id,
			&full_box_endpoint_bridge
		) != 0) {
		return 248;
	}
	struct prototype_hott_action_request box_square_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = box_identity_certificate->data.identity_type.
				identity_type_is_type_claim_id,
			.source_bridge_id = full_box_endpoint_bridge
		}
	};
	uint32_t box_square_identity_id;
	uint32_t box_square_identity_result_id;
	int box_square_intern_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			box_square_identity,
			&box_square_identity_id
		);
	int box_square_execute_status = box_square_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_square_identity_id,
			&box_square_identity_result_id
		) : -1;
	if (box_square_intern_status != 0 || box_square_execute_status != 0 ||
		action_results[box_square_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			box_square_identity_result_id
		].certificate_id >= action_db.certificate_count) {
		return 249;
	}
	const struct prototype_hott_action_certificate* box_square_certificate =
		&action_certificates[action_results[
			box_square_identity_result_id
		].certificate_id];
	uint32_t generated_box_square_type = box_square_certificate->data.
		identity_type.generated_type_declaration_id;
	const struct prototype_type_declaration* generated_box_square =
		&type_db.type_declarations[generated_box_square_type];
	const struct prototype_type_constructor_declaration* box_square_constructor =
		&type_db.constructor_declarations[generated_box_square->first_constructor];
	uint32_t box_square_fields[64];
	uint32_t box_square_field_count;
	if (box_square_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT ||
		generated_box_square->index_count != 8 ||
		generated_box_square->constructor_count != 1 ||
		prototype_context_extension_path(
			&context_db,
			box_square_constructor->parameter_context,
			box_square_constructor->field_context,
			box_square_fields,
			64,
			&box_square_field_count
		) != 0 || box_square_field_count != 9) {
		return 250;
	}
	uint32_t box_square_boundary[8] = {
		box_false,
		box_false,
		box_identity_witness,
		box_false,
		box_false,
		box_identity_witness,
		box_identity_witness,
		box_identity_witness
	};
	uint32_t box_square_type;
	uint32_t box_square_witness;
	uint32_t box_square_witness_claim;
	uint32_t box_square_field_claims[9] = {
		bool_false_constructor_claim,
		bool_false_constructor_claim,
		false_identity_witness_claim,
		bool_false_constructor_claim,
		bool_false_constructor_claim,
		false_identity_witness_claim,
		false_identity_witness_claim,
		false_identity_witness_claim,
		false_square_claim
	};
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_box_square_type,
			box_square_boundary,
			8,
			&box_square_type
		) != 0 || prototype_term_constructor(
			&term_db,
			box_square_certificate->data.identity_type.backing_type_former_term_id,
			0,
			&box_square_witness
		) != 0) {
		return 251;
	}
	uint32_t box_square_field_terms[9] = {
		bool_false,
		bool_false,
		false_identity_witness,
		bool_false,
		bool_false,
		false_identity_witness,
		false_identity_witness,
		false_identity_witness,
		false_square_witness
	};
	for (uint32_t i = 0; i < 9; ++i) {
		if (prototype_term_app(
				&term_db,
				box_square_witness,
				box_square_field_terms[i],
				&box_square_witness
			) != 0) {
			return 252;
		}
	}
	int box_square_claim_status = prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			box_square_witness,
			box_square_type,
			box_square_field_claims,
			9,
			&box_square_witness_claim
		);
	int box_square_validation_status = prototype_hott_action_db_validate(
		&action_db, &kernel_view, &bridge_db
	);
	if (box_square_claim_status != 0 || box_square_validation_status != 0) {
		return 253;
	}
	uint32_t box_identity_root_id;
	uint32_t concrete_box_identity_root_id;
	size_t box_identity_root_mark = identity_interface.identity_root_count;
	int box_root_status = prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			box_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&box_identity_root_id
		);
	int concrete_box_root_status = box_root_status == 0 ?
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			box_is_type,
			box_degeneracy_family_claim,
			box_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_box_identity_root_id
		) : -1;
	if (box_root_status != 0 || box_identity_root_id != box_identity_root_mark ||
		concrete_box_root_status != 0 || concrete_box_identity_root_id !=
			box_identity_root_mark + 1) {
		fprintf(
			stderr,
			"Box identity root failed generated_status=%d generated_id=%u "
			"concrete_status=%d concrete_id=%u root_count=%zu\n",
			box_root_status,
			box_identity_root_id,
			concrete_box_root_status,
			concrete_box_identity_root_id,
			identity_interface.identity_root_count
		);
		return 243;
	}
	struct prototype_judgement_proposition box_delta_propositions[8];
	struct prototype_judgement_derivation_candidate box_delta_candidates[8];
	struct prototype_judgement_candidate_premise box_delta_premises[
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result box_delta_motives[1];
	struct prototype_judgement_computation_constraint box_delta_constraints[1];
	struct prototype_judgement_effect_row_constraint box_delta_effect_rows[1];
	struct prototype_judgement_delta box_delta;
	prototype_judgement_delta_init(
		&box_delta,
		&judgement,
		box_delta_propositions,
		box_delta_candidates,
		8,
		box_delta_premises,
		8 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		box_delta_motives,
		1,
		box_delta_constraints,
		1,
		box_delta_effect_rows,
		1
	);
	prototype_judgement_delta_set_context_store(
		&box_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_context(&box_delta, empty);
	struct prototype_judgement_selected_evidence box_argument_evidence[3];
	uint32_t box_argument_occurrences[3] = {
		false_operation, false_operation, PROTOTYPE_INVALID_ID
	};
	if (prototype_judgement_delta_select_evidence(
			&box_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&box_argument_evidence[0]
		) != 0 || prototype_judgement_delta_select_evidence(
			&box_delta,
			false_operation,
			empty,
			bool_false,
			bool_view,
			&box_argument_evidence[1]
		) != 0 || prototype_judgement_delta_select_evidence(
			&box_delta,
			PROTOTYPE_INVALID_ID,
			empty,
			false_identity_witness,
			false_identity_type,
			&box_argument_evidence[2]
		) != 0 || prototype_judgement_delta_record_constructor_spine(
			&box_delta,
			&term_db,
			&type_db,
			box_identity_witness,
			box_identity_type,
			box_argument_occurrences,
			box_argument_evidence,
			3
		) != 0 || box_delta.proposition_count != 1 ||
		box_delta.derivation_candidate_count != 1 ||
		box_delta.derivation_candidates[0].proof_kind !=
			PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION ||
		box_delta.propositions[0].classifier != box_identity_type) {
		return 175;
	}
	uint32_t indexed_left_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_right_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_left_context;
	uint32_t indexed_right_context;
	uint32_t indexed_left_var;
	uint32_t indexed_right_var;
	uint32_t indexed_relation_args[2];
	uint32_t indexed_relation_type;
	uint32_t indexed_proof_binding;
	uint32_t indexed_proof_context;
	uint32_t indexed_proof_var;
	if (indexed_left_binding == PROTOTYPE_INVALID_ID ||
		indexed_right_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			empty,
			indexed_left_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&indexed_left_context
		) != 0 || prototype_context_extend(
			&context_db,
			indexed_left_context,
			indexed_right_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&indexed_right_context
		) != 0 || prototype_term_var(
			&term_db, indexed_left_binding, &indexed_left_var
		) != 0 || prototype_term_var(
			&term_db, indexed_right_binding, &indexed_right_var
		) != 0) {
		return 142;
	}
	indexed_relation_args[0] = indexed_left_var;
	indexed_relation_args[1] = indexed_right_var;
	indexed_proof_binding = prototype_term_new_binding(&term_db);
	if (indexed_proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			indexed_relation_args,
			2,
			&indexed_relation_type
		) != 0 || prototype_context_extend(
			&context_db,
			indexed_right_context,
			indexed_proof_binding,
			indexed_relation_type,
			PROTOTYPE_INVALID_ID,
			&indexed_proof_context
		) != 0 || prototype_term_var(
			&term_db, indexed_proof_binding, &indexed_proof_var
		) != 0) {
		return 142;
	}
	uint32_t refined_context;
	uint32_t refinement_substitution;
	uint32_t refined_constructor;
	uint32_t refined_left;
	uint32_t refined_right;
	uint32_t refined_proof;
	if (prototype_judgement_identity_relation_branch_refinement(
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			indexed_proof_context,
			indexed_proof_var,
			indexed_relation_type,
			0,
			indexed_proof_context,
			NULL,
			0,
			&refined_context,
			&refinement_substitution,
			&refined_constructor
		) != 0 || refined_context != empty || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_left_var,
			refinement_substitution,
			&refined_left
		) != 0 || refined_left != bool_false || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_right_var,
			refinement_substitution,
			&refined_right
		) != 0 || refined_right != bool_false || prototype_term_reindex(
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_proof_var,
			refinement_substitution,
			&refined_proof
		) != 0 || refined_proof != refined_constructor ||
		prototype_substitution_db_validate_classifier_coherence(
			&substitution_db, &context_db, &term_db, &type_db
		) != 0) {
		return 142;
	}
	struct prototype_judgement_proposition indexed_match_propositions[64];
	struct prototype_judgement_derivation_candidate indexed_match_candidates[64];
	struct prototype_judgement_claim indexed_match_claims[64];
	struct prototype_judgement_derivation indexed_match_derivations[64];
	struct prototype_judgement_candidate_premise indexed_match_candidate_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_premise_edge indexed_match_accepted_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_db indexed_match_judgement;
	prototype_judgement_db_init(
		&indexed_match_judgement,
		indexed_match_propositions,
		indexed_match_candidates,
		indexed_match_claims,
		indexed_match_derivations,
		64,
		indexed_match_candidate_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		indexed_match_accepted_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	uint32_t indexed_match_bool_type_claim;
	uint32_t indexed_match_projection;
	uint32_t indexed_match_bool_type_in_context_claim;
	uint32_t indexed_left_assumption_claim;
	if (prototype_judgement_add_type_formation_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			empty,
			bool_view,
			universe,
			&indexed_match_bool_type_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			indexed_proof_context,
			empty,
			&indexed_match_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_match_bool_type_claim,
			indexed_match_projection,
			&indexed_match_bool_type_in_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&indexed_match_judgement,
			&term_db,
			&context_db,
			indexed_proof_context,
			indexed_left_binding,
			bool_view,
			&indexed_left_assumption_claim
		) != 0) {
		return 143;
	}
	struct prototype_match_case_input indexed_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = PROTOTYPE_INVALID_ID
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t indexed_false_return;
	uint32_t indexed_true_return;
	uint32_t indexed_branch_classifier;
	uint32_t indexed_match_term;
	if (prototype_term_return(
			&term_db, indexed_left_var, &indexed_false_return
		) != 0 || prototype_term_return(
			&term_db, indexed_left_var, &indexed_true_return
		) != 0 || prototype_term_computation_type(
			&term_db, empty_effect_row, bool_view, &indexed_branch_classifier
		) != 0) {
		return 143;
	}
	indexed_match_cases[0].body = indexed_false_return;
	indexed_match_cases[1].body = indexed_true_return;
	if (prototype_term_match(
			&term_db,
			indexed_proof_var,
			indexed_match_cases,
			2,
			&indexed_match_term
		) != 0) {
		return 143;
	}
	struct prototype_typed_occurrence indexed_match_operations[6];
	struct prototype_typed_occurrence_edge indexed_match_operation_edges[8];
	struct prototype_typed_occurrence_match_case indexed_match_operation_cases[2];
	struct prototype_typed_occurrence_graph indexed_match_operation_graph;
	memset(&indexed_match_operation_graph, 0, sizeof(indexed_match_operation_graph));
	indexed_match_operation_graph.occurrences = indexed_match_operations;
	indexed_match_operation_graph.occurrence_capacity = 6;
	indexed_match_operation_graph.edges = indexed_match_operation_edges;
	indexed_match_operation_graph.edge_capacity = 8;
	indexed_match_operation_graph.cases = indexed_match_operation_cases;
	indexed_match_operation_graph.case_capacity = 2;
	uint32_t indexed_scrutinee_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_proof_var,
		indexed_relation_type,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t indexed_false_body_occurrence = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_left_var,
		bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t indexed_false_return_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_false_return,
		indexed_branch_classifier,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t indexed_true_body_occurrence = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_left_var,
		bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t indexed_true_return_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_true_return,
		indexed_branch_classifier,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t indexed_match_operation = add_operation(
		&indexed_match_operation_graph,
		indexed_proof_context,
		indexed_match_term,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	indexed_match_operations[indexed_scrutinee_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_VAR;
