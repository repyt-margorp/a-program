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
	const struct prototype_term* true_true_identity_term =
		true_true_identity_family && true_true_identity_family->subject <
			term_db.term_count ? &term_db.terms[
			true_true_identity_family->subject
		] : NULL;
	const struct prototype_term* true_false_identity_term =
		true_false_identity_family && true_false_identity_family->subject <
			term_db.term_count ? &term_db.terms[
			true_false_identity_family->subject
		] : NULL;
	const struct prototype_term* true_true_left_application =
		true_true_identity_term && true_true_identity_term->tag ==
			PROTOTYPE_TERM_APP && true_true_identity_term->as.app.function <
			term_db.term_count ? &term_db.terms[
			true_true_identity_term->as.app.function
		] : NULL;
	const struct prototype_term* true_false_left_application =
		true_false_identity_term && true_false_identity_term->tag ==
			PROTOTYPE_TERM_APP && true_false_identity_term->as.app.function <
			term_db.term_count ? &term_db.terms[
			true_false_identity_term->as.app.function
		] : NULL;
	if (!true_true_identity_family || !true_false_identity_family ||
		true_true_identity_family->subject == true_false_identity_family->subject ||
		!true_true_identity_term || !true_false_identity_term ||
		true_true_identity_term->tag != PROTOTYPE_TERM_APP ||
		true_false_identity_term->tag != PROTOTYPE_TERM_APP ||
		!true_true_left_application || !true_false_left_application ||
		true_true_left_application->tag != PROTOTYPE_TERM_APP ||
		true_false_left_application->tag != PROTOTYPE_TERM_APP ||
		true_true_left_application->as.app.function !=
			bool_identity_certificate->data.identity_type.
				backing_type_former_term_id ||
		true_false_left_application->as.app.function !=
			true_true_left_application->as.app.function ||
		true_true_left_application->as.app.argument != bool_true ||
		true_true_identity_term->as.app.argument != bool_true ||
		true_false_left_application->as.app.argument != bool_true ||
		true_false_identity_term->as.app.argument != bool_false) {
		return 317;
	}
	uint32_t true_degeneracy_family_claim;
	uint32_t true_degeneracy_witness;
	uint32_t true_degeneracy_witness_claim;
	uint32_t checked_true_identity_family_claim;
	uint32_t checked_true_witness_claim;
	int true_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			&true_degeneracy_family_claim,
			&true_degeneracy_witness,
			&true_degeneracy_witness_claim
		);
	int true_check_status = true_degeneracy_status == 0 ?
		prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			true_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) : -1;
	int false_check_status = true_degeneracy_status == 0 ?
		prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			true_claim,
			false_claim,
			true_degeneracy_witness_claim,
			&checked_true_identity_family_claim,
			&checked_true_witness_claim
		) : -1;
	if (true_degeneracy_status != 0 || true_check_status != 0 ||
		false_check_status == 0) {
		fprintf(
			stderr,
			"dimension action Bool degeneracy failed construct=%d true=%d false=%d\n",
			true_degeneracy_status,
			true_check_status,
			false_check_status
		);
		return 318;
	}
	(void)true_degeneracy_family_claim;
	(void)true_degeneracy_witness;
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
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT) {
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
			PROTOTYPE_TERM_DIMENSION_ACTION) {
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
	size_t square_type_count_before = type_db.semantic_schema.type_count;
	size_t square_constructor_count_before = type_db.semantic_schema.constructor_count;
	uint32_t square_identity_computation_id = PROTOTYPE_INVALID_ID;
	uint32_t square_identity_result_id = PROTOTYPE_INVALID_ID;
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
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION ||
		type_db.semantic_schema.type_count != square_type_count_before ||
		type_db.semantic_schema.constructor_count != square_constructor_count_before) {
		fprintf(
			stderr,
			"square action gate failed result=%u count=%zu state=%d certificate=%u "
			"rule=%d types=%zu/%zu constructors=%zu/%zu\n",
			square_identity_result_id,
			action_db.result_count,
			square_identity_result_id < action_db.result_count ?
				action_results[square_identity_result_id].outcome.state : -1,
			square_identity_result_id < action_db.result_count ?
				action_results[square_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			square_identity_result_id < action_db.result_count && action_results[
				square_identity_result_id
			].certificate_id < action_db.certificate_count ? action_certificates[
				action_results[square_identity_result_id].certificate_id
			].data.identity_type.computation_rule : -1,
			type_db.semantic_schema.type_count,
			square_type_count_before,
			type_db.semantic_schema.constructor_count,
			square_constructor_count_before
		);
		return 255;
	}
	uint32_t square_identity_family = action_certificates[
		action_results[square_identity_result_id].certificate_id
	].data.identity_type.backing_type_former_term_id;
	uint32_t square_source;
	uint32_t square_operator;
	uint32_t square_dimension;
	if (prototype_term_dimension_action_info(
			&term_db,
			square_identity_family,
			&square_source,
			&square_operator
		) != 0 || prototype_dimension_action_term_dimension(
			&term_db,
			&bridge_dimension_operator_db,
			square_identity_family,
			&square_dimension
		) != 0 || square_dimension != 2) {
		return 255;
	}
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
		NULL,
		0,
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
	uint32_t bool_identity_family =
		bool_identity_certificate->data.identity_type.
			backing_type_former_term_id;
	uint32_t false_identity_type;
	uint32_t false_true_identity_type;
	uint32_t false_identity_witness;
	uint32_t false_identity_type_claim;
	uint32_t false_identity_witness_claim;
	uint32_t false_true_left;
	uint32_t false_true_left_claim;
	uint32_t false_true_type_claim;
	uint32_t false_true_is_type_claim;
	uint32_t ignored_false_true_claim;
	if (prototype_term_app(
			&term_db, bool_identity_family, bool_false, &false_identity_type
		) != 0 || prototype_term_app(
			&term_db, false_identity_type, bool_false, &false_identity_type
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
		)->classifier != false_identity_type || fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			bool_identity_certificate->data.identity_type.
				backing_type_former_has_type_claim_id,
			bool_false_constructor_claim,
			&false_true_left,
			&false_true_left_claim
		) != 0 || fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			false_true_left_claim,
			bool_true_constructor_claim,
			&false_true_identity_type,
			&false_true_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			false_true_identity_type,
			universe,
			false_true_type_claim,
			&false_true_is_type_claim
		) != 0 || prototype_judgement_add_dimension_action_term(
			&judgement,
			&term_db,
			&bridge_dimension_operator_db,
			empty,
			false_identity_witness,
			false_true_identity_type,
			bool_false_constructor_claim,
			bool_is_type,
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
	false_square_type = square_identity_family;
	mixed_square_type = square_identity_family;
	for (uint32_t i = 0; i < 8; ++i) {
		if (prototype_term_app(
				&term_db,
				false_square_type,
				false_square_boundary[i],
				&false_square_type
			) != 0 || prototype_term_app(
				&term_db,
				mixed_square_type,
				mixed_square_boundary[i],
				&mixed_square_type
			) != 0) {
			return 246;
		}
	}
	uint32_t false_square_witness = PROTOTYPE_INVALID_ID;
	uint32_t false_square_claim = PROTOTYPE_INVALID_ID;
	if (false_square_type == mixed_square_type) {
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
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION) {
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
		term_db.terms[higher_identity_witness].tag !=
			PROTOTYPE_TERM_DIMENSION_ACTION) {
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
	false_square_witness = higher_identity_witness;
	false_square_claim = higher_identity_witness_claim;
	(void)false_square_claim;
	uint32_t concrete_identity_root_id;
	if (prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
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
		) != 0 || prototype_term_total_computation_type(
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
	size_t thunk_identity_type_count_before = type_db.semantic_schema.type_count;
	size_t thunk_identity_constructor_count_before = type_db.semantic_schema.constructor_count;
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
	uint32_t thunk_identity_source;
	uint32_t thunk_identity_operator;
	uint32_t thunk_identity_dimension;
	if (thunk_bool_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN ||
		type_db.semantic_schema.type_count != thunk_identity_type_count_before ||
		type_db.semantic_schema.constructor_count != thunk_identity_constructor_count_before ||
		prototype_term_dimension_action_info(
			&term_db,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			&thunk_identity_source,
			&thunk_identity_operator
		) != 0 || thunk_identity_source != thunk_bool_type ||
		thunk_identity_operator != bridge_db.bridges[
			bridge
		].dimension_operator_id ||
		prototype_dimension_action_term_dimension(
			&term_db,
			&bridge_dimension_operator_db,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			&thunk_identity_dimension
		) != 0 || thunk_identity_dimension != 1 ||
		prototype_hott_action_db_validate(
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
	uint32_t false_return_claim;
	uint32_t false_thunk_claim;
	uint32_t true_return_claim;
	uint32_t true_thunk_claim;
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
		) != 0 || prototype_judgement_add_return_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			true_return,
			pure_bool_computation_type,
			bool_true_constructor_claim,
			&true_return_claim
		) != 0 || prototype_judgement_add_thunk_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			true_thunk,
			thunk_bool_type,
			true_return_claim,
			&true_thunk_claim
		) != 0) {
		return 179;
	}
	uint32_t thunk_false_left_claim;
	uint32_t thunk_false_true_left_claim;
	uint32_t thunk_false_identity_type_claim;
	uint32_t thunk_false_true_identity_type_claim;
	uint32_t thunk_false_identity_is_type_claim;
	uint32_t thunk_false_true_identity_is_type_claim;
	if (fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_has_type_claim_id,
			false_thunk_claim,
			&thunk_false_identity_type,
			&thunk_false_left_claim
		) != 0 || fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			thunk_false_left_claim,
			false_thunk_claim,
			&thunk_false_identity_type,
			&thunk_false_identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			thunk_false_identity_type,
			universe,
			thunk_false_identity_type_claim,
			&thunk_false_identity_is_type_claim
		) != 0 || fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			thunk_bool_identity_certificate->data.identity_type.
				backing_type_former_has_type_claim_id,
			false_thunk_claim,
			&thunk_false_true_identity_type,
			&thunk_false_true_left_claim
		) != 0 || fixture_apply_claim_argument(
			&kernel_builder,
			empty,
			thunk_false_true_left_claim,
			true_thunk_claim,
			&thunk_false_true_identity_type,
			&thunk_false_true_identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			thunk_false_true_identity_type,
			universe,
			thunk_false_true_identity_type_claim,
			&thunk_false_true_identity_is_type_claim
		) != 0 || thunk_false_identity_type ==
			thunk_false_true_identity_type) {
		return 180;
	}
	uint32_t thunk_degeneracy_family_claim;
	uint32_t thunk_degeneracy_witness;
	uint32_t thunk_degeneracy_witness_claim;
	uint32_t expected_thunk_degeneracy_witness;
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
		prototype_term_dimension_action(
			&term_db,
			&bridge_dimension_operator_db,
			false_thunk,
			thunk_identity_operator,
			&expected_thunk_degeneracy_witness
		) != 0 || thunk_degeneracy_witness !=
			expected_thunk_degeneracy_witness) {
		return 222;
	}
	int found_thunk_dimension_action = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				thunk_degeneracy_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM &&
			judgement.derivations[i].premise_count == 2) {
			found_thunk_dimension_action = 1;
			break;
		}
	}
	if (!found_thunk_dimension_action || prototype_judgement_add_dimension_action_term(
			&judgement,
			&term_db,
			&bridge_dimension_operator_db,
			empty,
			thunk_degeneracy_witness,
			thunk_false_true_identity_type,
			false_thunk_claim,
			thunk_bool_is_type,
			&(uint32_t) { PROTOTYPE_INVALID_ID }
		) == 0) {
		return 223;
	}
	uint32_t thunk_certificate_id = action_results[
		thunk_bool_identity_result_id
	].certificate_id;
	uint32_t saved_thunk_identity_type = action_db.certificates[
		thunk_certificate_id
	].data.identity_type.identity_type_term_id;
	action_db.certificates[
		thunk_certificate_id
	].data.identity_type.identity_type_term_id = thunk_false_true_identity_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 182;
	}
	action_db.certificates[
		thunk_certificate_id
	].data.identity_type.identity_type_term_id = saved_thunk_identity_type;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 183;
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
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
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
		].tag != PROTOTYPE_TERM_DIMENSION_ACTION || constant_thunk_witness !=
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
			&bridge_dimension_operator_db,
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
			&bridge_dimension_operator_db,
			&judgement
		) != 0) {
		return 228;
	}
	uint32_t saved_thunk_root_family_claim = identity_roots[
		concrete_thunk_identity_root_id
	].identity_family_has_type_claim_id;
	identity_roots[
		concrete_thunk_identity_root_id
	].identity_family_has_type_claim_id = thunk_bool_has_type;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement
		) == 0) {
		return 230;
	}
	identity_roots[
		concrete_thunk_identity_root_id
	].identity_family_has_type_claim_id = saved_thunk_root_family_claim;
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
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
	uint32_t bool_function_identity_result_id = PROTOTYPE_INVALID_ID;
	int bool_function_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_function_identity_computation,
			&bool_function_identity_computation_id
		);
	int bool_function_execution_status = bool_function_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_computation_id,
			&bool_function_identity_result_id
		) : -1;
	if (bool_function_request_status != 0 || bool_function_execution_status != 0 ||
		bool_function_identity_result_id >= action_db.result_count ||
		action_results[bool_function_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[bool_function_identity_result_id].certificate_id >=
			action_db.certificate_count) {
		fprintf(
			stderr,
			"bool function identity request=%d execute=%d result=%u/%zu state=%d "
			"reason=%d certificate=%u/%zu validate=%d\n",
			bool_function_request_status,
			bool_function_execution_status,
			bool_function_identity_result_id,
			action_db.result_count,
			bool_function_identity_result_id < action_db.result_count ?
				action_results[bool_function_identity_result_id].outcome.state : -1,
			bool_function_identity_result_id < action_db.result_count ?
				action_results[bool_function_identity_result_id].outcome.
					residual_reason : -1,
			bool_function_identity_result_id < action_db.result_count ?
				action_results[bool_function_identity_result_id].certificate_id :
				PROTOTYPE_INVALID_ID,
			action_db.certificate_count,
			prototype_hott_action_db_validate(
				&action_db, &kernel_view, &bridge_db
			)
		);
		return 185;
	}
	struct prototype_hott_action_certificate* bool_function_identity_certificate =
		&action_db.certificates[
			action_results[bool_function_identity_result_id].certificate_id
		];
	if (bool_function_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE ||
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
			&bridge_dimension_operator_db,
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
	uint32_t checked_constant_function_family_claim = PROTOTYPE_INVALID_ID;
	uint32_t checked_constant_function_witness_claim = PROTOTYPE_INVALID_ID;
	int constant_function_degeneracy_status = prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			&constant_function_degeneracy_family_claim,
			&constant_function_degeneracy_witness,
			&constant_function_degeneracy_witness_claim
		);
	int checked_constant_function_status = constant_function_degeneracy_status == 0 ?
		prototype_hott_check_object_identity_witness(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_function_identity_result_id,
			constant_function_value_claim,
			matched_constant_function_value_claim,
			constant_function_degeneracy_witness_claim,
			&checked_constant_function_family_claim,
			&checked_constant_function_witness_claim
		) : -1;
	if (constant_function_degeneracy_status != 0 ||
		checked_constant_function_status != 0 ||
		checked_constant_function_family_claim !=
			constant_function_identity_family_claim) {
		fprintf(
			stderr,
			"constant function identity degeneracy=%d check=%d family=%u/%u\n",
			constant_function_degeneracy_status,
			checked_constant_function_status,
			checked_constant_function_family_claim,
			constant_function_identity_family_claim
		);
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
	if (prototype_term_total_computation_type(
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
		fprintf(
			stderr,
			"nested Pi carrier identity computation failed intern=%d execute=%d "
			"request=%u result=%u "
			"state=%d result_request=%u certificate=%u rule=%d\n",
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
					].data.identity_type.computation_rule : -1
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
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE ||
		prototype_hott_action_db_validate(
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
	int pi_variable_intern_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			pi_variable_identity_computation,
			&pi_variable_identity_computation_id
		);
	int pi_variable_execute_status = pi_variable_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			pi_variable_identity_computation_id,
			&pi_variable_identity_result_id
		) : -1;
	if (pi_variable_intern_status != 0 || pi_variable_execute_status != 0 ||
		action_results[pi_variable_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_db.certificates[
			action_results[pi_variable_identity_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		fprintf(
			stderr,
			"Pi variable identity failed intern=%d execute=%d result=%u state=%d rule=%d\n",
			pi_variable_intern_status,
			pi_variable_execute_status,
			pi_variable_identity_result_id,
			pi_variable_identity_result_id < action_db.result_count ?
				action_results[pi_variable_identity_result_id].outcome.state : -1,
			pi_variable_identity_result_id < action_db.result_count &&
				action_results[pi_variable_identity_result_id].certificate_id <
					action_db.certificate_count ? action_db.certificates[
						action_results[pi_variable_identity_result_id].certificate_id
					].data.identity_type.computation_rule : -1
		);
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
		fprintf(
			stderr,
			"Pi replay mutated TermDB terms=%zu/%zu bindings=%u/%u\n",
			term_db.term_count,
			pi_replay_term_count,
			term_db.next_binding_id,
			pi_replay_next_binding
		);
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
		) != 0 || pi_ap_identity != false_identity_witness) {
		fprintf(
			stderr,
			"Pi ap/refl law failed left=%u right=%u result=%u expected=%u\n",
			pi_ap_left,
			pi_ap_right,
			pi_ap_identity,
			false_identity_witness
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
	uint32_t pi_identity_root_id = PROTOTYPE_INVALID_ID;
	uint32_t concrete_pi_identity_root_id = PROTOTYPE_INVALID_ID;
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
			&bridge_dimension_operator_db,
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
			&bridge_dimension_operator_db,
			&judgement
		) != 0) {
		fprintf(
			stderr,
			"Pi identity root publication failed root=%u concrete=%u count=%zu "
			"validation=%d\n",
			pi_identity_root_id,
			concrete_pi_identity_root_id,
			identity_interface.identity_root_count,
			prototype_artifact_interface_validate_identity_roots(
				&identity_interface,
				&term_db,
				&type_db,
				&context_db,
				&bridge_dimension_operator_db,
				&judgement
			)
		);
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
			&bridge_dimension_operator_db,
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
			&bridge_dimension_operator_db,
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
		&kernel_builder,
		&term_db,
		&type_db,
		&context_db,
		&substitution_db,
		&judgement,
		&bridge_dimension_operator_db,
		&operation_graph,
		empty,
		universe,
		bool_view,
		bool_false,
		bool_true,
		bool_identity_certificate->data.identity_type.
			backing_type_former_term_id,
		bool_identity_certificate->data.identity_type.
			backing_type_former_has_type_claim_id,
		thunk_bool_identity_certificate->data.identity_type.
			backing_type_former_term_id,
		thunk_bool_identity_certificate->data.identity_type.
			backing_type_former_has_type_claim_id,
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
			&bridge_dimension_operator_db,
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
			&bridge_dimension_operator_db,
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
	size_t box_type_count_before = type_db.semantic_schema.type_count;
	uint32_t box_identity_computation_id;
	uint32_t box_identity_computation_result_id;
	int box_request_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			box_identity_computation,
			&box_identity_computation_id
		);
	int box_execution_status = box_request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_id,
			&box_identity_computation_result_id
		) : -1;
	if (box_request_status != 0 || box_execution_status != 0 ||
		action_results[box_identity_computation_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[box_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		fprintf(stderr,
			"Box identity failed request=%d execute=%d result=%u state=%d "
			"residual=%d certificate=%u count=%zu\n",
			box_request_status, box_execution_status,
			box_execution_status == 0 ? box_identity_computation_result_id :
				PROTOTYPE_INVALID_ID,
			box_execution_status == 0 ? action_results[
				box_identity_computation_result_id
			].outcome.state : -1,
			box_execution_status == 0 ? action_results[
				box_identity_computation_result_id
			].outcome.residual_reason : -1,
			box_execution_status == 0 ? action_results[
				box_identity_computation_result_id
			].certificate_id : PROTOTYPE_INVALID_ID,
			action_db.certificate_count);
		return 170;
	}
	const struct prototype_hott_action_certificate* box_identity_certificate =
		&action_db.certificates[
			action_results[box_identity_computation_result_id].certificate_id
		];
	uint32_t box_action_source;
	uint32_t box_action_operator;
	struct prototype_type_schema_view box_schema;
	uint32_t box_constructor_classifier;
	uint32_t box_constructor_field_count = 0;
	if (box_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		type_db.semantic_schema.type_count != box_type_count_before ||
		prototype_term_dimension_action_info(
			&term_db,
			box_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			&box_action_source,
			&box_action_operator
		) != 0 || box_action_source != box_view ||
		prototype_type_schema_view_query(
			&type_db.semantic_schema,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			box_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			&box_schema
		) != 0 || prototype_constructor_schema_view_action_classifier(
			&type_db,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			&box_schema,
			0,
			&box_constructor_classifier
		) != 0 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 171;
	}
	for (uint32_t cursor = box_constructor_classifier;
		cursor < term_db.term_count &&
		term_db.terms[cursor].tag == PROTOTYPE_TERM_PI;) {
		uint32_t binding;
		uint32_t body;
		if (prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[cursor].as.pi.codomain_family,
				&binding,
				&body
			) != 0) {
			return 171;
		}
		box_constructor_field_count++;
		cursor = body;
	}
	if (box_constructor_field_count != 3) {
		return 171;
	}
	struct prototype_hott_action_request dependent_box_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = dependent_box_is_type,
			.source_bridge_id = bridge
		}
	};
	size_t dependent_box_type_count_before = type_db.semantic_schema.type_count;
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
	uint32_t dependent_box_action_source;
	uint32_t dependent_box_action_operator;
	struct prototype_type_schema_view dependent_box_schema;
	uint32_t dependent_box_constructor_classifier;
	uint32_t dependent_box_constructor_field_count = 0;
	if (type_db.semantic_schema.type_count != dependent_box_type_count_before ||
		prototype_term_dimension_action_info(
			&term_db,
			dependent_box_identity->data.identity_type.
				backing_type_former_term_id,
			&dependent_box_action_source,
			&dependent_box_action_operator
		) != 0 || dependent_box_action_source != dependent_box_view ||
		prototype_type_schema_view_query(
			&type_db.semantic_schema,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			dependent_box_identity->data.identity_type.
				backing_type_former_term_id,
			&dependent_box_schema
		) != 0 || prototype_constructor_schema_view_action_classifier(
			&type_db,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			&dependent_box_schema,
			0,
			&dependent_box_constructor_classifier
		) != 0) {
		return 310;
	}
	for (uint32_t cursor = dependent_box_constructor_classifier;
		cursor < term_db.term_count &&
		term_db.terms[cursor].tag == PROTOTYPE_TERM_PI;) {
		uint32_t binding;
		uint32_t body;
		if (prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[cursor].as.pi.codomain_family,
				&binding,
				&body
			) != 0) {
			return 310;
		}
		dependent_box_constructor_field_count++;
		cursor = body;
	}
	if (dependent_box_constructor_field_count != 6 ||
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
	uint32_t dependent_witness_head = dependent_box_witness;
	uint32_t dependent_witness_source;
	uint32_t dependent_witness_operator;
	uint32_t dependent_source_head;
	uint32_t dependent_source_owner;
	uint32_t dependent_source_ordinal;
	uint32_t dependent_witness_arguments[6];
	const struct prototype_judgement_proposition* dependent_identity_family =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_family_claim
		);
	const struct prototype_judgement_proposition* dependent_witness_claim =
		prototype_judgement_claim_proposition(
			&judgement, dependent_box_witness_claim
		);
	int dependent_witness_spine_status = 0;
	for (uint32_t i = 6; i > 0; --i) {
		if (dependent_witness_head >= term_db.term_count || term_db.terms[
			dependent_witness_head
		].tag != PROTOTYPE_TERM_APP) {
			dependent_witness_spine_status = -1;
			break;
		}
		dependent_witness_arguments[i - 1] = term_db.terms[
			dependent_witness_head
		].as.app.argument;
		dependent_witness_head = term_db.terms[
			dependent_witness_head
		].as.app.function;
	}
	uint32_t dependent_source_argument_count;
	if (!dependent_identity_family || !dependent_witness_claim ||
		dependent_witness_claim->classifier != dependent_identity_family->subject ||
		dependent_witness_spine_status != 0 ||
		prototype_term_dimension_action_info(
			&term_db,
			dependent_witness_head,
			&dependent_witness_source,
			&dependent_witness_operator
		) != 0 || dependent_witness_operator != dependent_box_action_operator ||
		prototype_term_constructor_spine_info(
			&term_db,
			dependent_witness_source,
			&dependent_source_head,
			&dependent_source_owner,
			&dependent_source_ordinal,
			NULL,
			0,
			&dependent_source_argument_count
		) != 0 || dependent_source_owner != dependent_box_view ||
		dependent_source_ordinal != 0 || dependent_source_argument_count != 0 ||
		dependent_witness_arguments[0] != bool_false ||
		dependent_witness_arguments[1] != bool_false ||
		dependent_witness_arguments[2] != false_identity_witness ||
		dependent_witness_arguments[3] != bool_false ||
		dependent_witness_arguments[4] != bool_false ||
		dependent_witness_arguments[5] != false_identity_witness) {
		return 312;
	}
	(void)dependent_source_head;
	uint32_t box_false;
	uint32_t box_false_right;
	uint32_t box_source_constructor;
	uint32_t box_identity_witness;
	uint32_t box_identity_type;
	if (prototype_term_constructor(
			&term_db, box_view, 0, &box_source_constructor
		) != 0 || prototype_term_app(
			&term_db, box_source_constructor, bool_false, &box_false
		) != 0 || prototype_term_app(
			&term_db, box_source_constructor, bool_false, &box_false_right
		) != 0 || prototype_term_dimension_action(
			&term_db,
			&bridge_dimension_operator_db,
			box_source_constructor,
			box_action_operator,
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
	if (prototype_term_app(
			&term_db,
			box_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			box_false,
			&box_identity_type
		) != 0 || prototype_term_app(
			&term_db,
			box_identity_type,
			box_false_right,
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
	size_t box_type_count_before_square = type_db.semantic_schema.type_count;
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
	uint32_t box_square_action_source;
	uint32_t box_square_action_operator;
	const struct prototype_dimension_operator* box_square_operator;
	struct prototype_type_schema_view box_square_schema;
	struct prototype_constructor_schema_view box_square_constructor_schema;
	uint32_t box_square_constructor_classifier;
	uint32_t box_square_field_count = 0;
	if (box_square_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION ||
		type_db.semantic_schema.type_count != box_type_count_before_square ||
		prototype_term_dimension_action_info(
			&term_db,
			box_square_certificate->data.identity_type.backing_type_former_term_id,
			&box_square_action_source,
			&box_square_action_operator
		) != 0 || box_square_action_source != box_identity_certificate->data.
			identity_type.backing_type_former_term_id || !(box_square_operator =
			prototype_dimension_operator_get(
				&bridge_dimension_operator_db, box_square_action_operator
			)) || box_square_operator->source_dimension != 1 ||
		box_square_operator->target_dimension != 2 || prototype_type_schema_view_query(
			&type_db.semantic_schema,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			box_square_certificate->data.identity_type.backing_type_former_term_id,
			&box_square_schema
		) != 0 || box_square_schema.target_dimension != 2 ||
		prototype_constructor_schema_view_query(
			&type_db.semantic_schema,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			&box_square_schema,
			0,
			&box_square_constructor_schema
		) != 0 || prototype_constructor_schema_view_action_classifier(
			&type_db,
			&context_db,
			&term_db,
			&bridge_dimension_operator_db,
			&box_square_schema,
			0,
			&box_square_constructor_classifier
		) != 0) {
		return 250;
	}
	for (uint32_t cursor = box_square_constructor_classifier;
		cursor < term_db.term_count && term_db.terms[cursor].tag ==
			PROTOTYPE_TERM_PI;) {
		uint32_t binding;
		uint32_t body;
		if (prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[cursor].as.pi.codomain_family,
				&binding,
				&body
			) != 0) {
			return 250;
		}
		box_square_field_count++;
		cursor = body;
	}
	if (box_square_field_count != 9) {
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
	box_square_type = box_square_certificate->data.identity_type.
		backing_type_former_term_id;
	for (uint32_t i = 0; i < 8; ++i) {
		if (prototype_term_app(
				&term_db, box_square_type, box_square_boundary[i], &box_square_type
			) != 0) {
			return 251;
		}
	}
	box_square_witness = box_square_constructor_schema.acted_constructor_term;
	uint32_t box_square_witness_source;
	uint32_t box_square_witness_operator;
	if (prototype_term_dimension_action_info(
			&term_db,
			box_square_witness,
			&box_square_witness_source,
			&box_square_witness_operator
		) != 0 || box_square_witness_operator != box_square_action_operator) {
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
	uint32_t box_square_witness_classifier = box_square_constructor_classifier;
	for (uint32_t i = 0; i < 9; ++i) {
		uint32_t binder;
		uint32_t body;
		if (box_square_witness_classifier >= term_db.term_count || term_db.terms[
				box_square_witness_classifier
			].tag != PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
				&term_db,
				term_db.terms[box_square_witness_classifier].as.pi.codomain_family,
				&binder,
				&body
			) != 0 || prototype_term_graph_substitute_bound_var(
				&term_db,
				&type_db,
				body,
				binder,
				box_square_field_terms[i],
				&box_square_witness_classifier
			) != 0 || prototype_term_app(
				&term_db,
				box_square_witness,
				box_square_field_terms[i],
				&box_square_witness
			) != 0) {
			return 252;
		}
	}
	int box_square_validation_status = prototype_hott_action_db_validate(
		&action_db, &kernel_view, &bridge_db
	);
	if (box_square_witness_classifier != box_square_type ||
		box_square_validation_status != 0) {
		return 253;
	}
	(void)box_square_witness_source;
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
			&bridge_dimension_operator_db,
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
		) != 0) {
		return 175;
	}
	int acted_constructor_as_source_status =
		prototype_judgement_delta_record_constructor_spine(
			&box_delta,
			&term_db,
			&type_db,
			box_identity_witness,
			box_identity_type,
			box_argument_occurrences,
			box_argument_evidence,
			3
		);
	if (acted_constructor_as_source_status == 0 || box_delta.proposition_count != 0 ||
		box_delta.derivation_candidate_count != 0) {
		return 175;
	}
	uint32_t indexed_left_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_right_binding = prototype_term_new_binding(&term_db);
	uint32_t indexed_left_context;
	uint32_t indexed_right_context;
	uint32_t indexed_left_var;
	uint32_t indexed_right_var;
	uint32_t indexed_relation_args[2];
	uint32_t indexed_relation_left_application;
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
	if (indexed_proof_binding == PROTOTYPE_INVALID_ID || prototype_term_app(
			&term_db,
			bool_identity_family,
			indexed_relation_args[0],
			&indexed_relation_left_application
		) != 0 || prototype_term_app(
			&term_db,
			indexed_relation_left_application,
			indexed_relation_args[1],
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
			&bridge_dimension_operator_db,
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
	uint32_t indexed_match_bool_is_type_claim;
	uint32_t indexed_action_classifier_has_type_claim;
	uint32_t indexed_action_classifier_is_type_claim;
	uint32_t indexed_action_family_claim;
	uint32_t indexed_action_family_projection;
	uint32_t indexed_action_family_in_right_context_claim;
	uint32_t indexed_left_in_right_context_claim;
	uint32_t indexed_right_in_right_context_claim;
	uint32_t indexed_relation_left_claim;
	uint32_t indexed_relation_type_claim;
	uint32_t indexed_relation_projection;
	uint32_t indexed_relation_type_in_proof_context_claim;
	uint32_t indexed_match_projection;
	uint32_t indexed_match_bool_type_in_context_claim;
	uint32_t indexed_left_assumption_claim;
	const struct prototype_judgement_proposition* bool_identity_family_proposition =
		prototype_judgement_claim_proposition(
			&judgement,
			bool_identity_certificate->data.identity_type.
				backing_type_former_has_type_claim_id
		);
	struct prototype_kernel_builder indexed_match_builder = kernel_builder;
	indexed_match_builder.judgement = &indexed_match_judgement;
	if (!bool_identity_family_proposition ||
		prototype_judgement_add_type_formation_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			empty,
			bool_view,
			universe,
			&indexed_match_bool_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&indexed_match_judgement,
			&term_db,
			empty,
			bool_view,
			universe,
			indexed_match_bool_type_claim,
			&indexed_match_bool_is_type_claim
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			bool_identity_family_proposition->classifier,
			universe,
			&indexed_action_classifier_has_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&indexed_match_judgement,
			&term_db,
			empty,
			bool_identity_family_proposition->classifier,
			universe,
			indexed_action_classifier_has_type_claim,
			&indexed_action_classifier_is_type_claim
		) != 0 || prototype_judgement_add_dimension_action_type_formation(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			empty,
			bool_identity_family,
			bool_identity_family_proposition->classifier,
			indexed_match_bool_is_type_claim,
			&indexed_action_family_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			indexed_right_context,
			empty,
			&indexed_action_family_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_action_family_claim,
			indexed_action_family_projection,
			&indexed_action_family_in_right_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&indexed_match_judgement,
			&term_db,
			&context_db,
			indexed_right_context,
			indexed_left_binding,
			bool_view,
			&indexed_left_in_right_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&indexed_match_judgement,
			&term_db,
			&context_db,
			indexed_right_context,
			indexed_right_binding,
			bool_view,
			&indexed_right_in_right_context_claim
		) != 0 || fixture_apply_claim_argument(
			&indexed_match_builder,
			indexed_right_context,
			indexed_action_family_in_right_context_claim,
			indexed_left_in_right_context_claim,
			&(uint32_t) { 0 },
			&indexed_relation_left_claim
		) != 0 || fixture_apply_claim_argument(
			&indexed_match_builder,
			indexed_right_context,
			indexed_relation_left_claim,
			indexed_right_in_right_context_claim,
			&(uint32_t) { 0 },
			&indexed_relation_type_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			indexed_proof_context,
			indexed_right_context,
			&indexed_relation_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&indexed_match_judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			indexed_relation_type_claim,
			indexed_relation_projection,
			&indexed_relation_type_in_proof_context_claim
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
		fprintf(stderr, "indexed match setup failed\n");
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
		) != 0 || prototype_term_total_computation_type(
			&term_db, empty_effect_row, bool_view, &indexed_branch_classifier
		) != 0) {
		fprintf(stderr, "indexed match term components failed\n");
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
		fprintf(stderr, "indexed match construction failed\n");
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
