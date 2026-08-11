	uint32_t pure_box_thunk_type_action_id;
	uint32_t pure_box_thunk_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pure_box_thunk_type_action,
			&pure_box_thunk_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, pure_box_thunk_type_action_id,
			&pure_box_thunk_type_result_id
		) != 0 || action_results[pure_box_thunk_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 84;
	}
	struct prototype_hott_action_request thunked_box_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = thunked_box_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = pure_box_thunk_type_action_id
		}
	};
	uint32_t thunked_box_action_id;
	uint32_t thunked_box_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, thunked_box_action,
			&thunked_box_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, thunked_box_action_id,
			&thunked_box_result_id
		) != 0 || action_results[thunked_box_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 85;
	}
	uint32_t thunked_box_witness_claim = action_certificates[
		action_results[thunked_box_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_thunk_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				thunked_box_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS &&
			judgement.derivations[i].premise_count == 4 &&
			judgement.derivations[i].premises[3].claim_id ==
				returned_box_witness_claim) {
			found_thunk_witness = 1;
			break;
		}
	}
	if (!found_thunk_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 86;
	}
	uint32_t bool_to_pure_bool;
	if (prototype_term_pi(
			&term_db, bool_view, pure_comp, &bool_to_pure_bool
		) != 0) {
		return 87;
	}
	uint32_t pi_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		bool_to_pure_bool,
		bool_context,
		PROTOTYPE_INVALID_ID,
		bool_to_pure_bool,
		universe
	);
	struct prototype_hott_action_request pi_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pi_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pi_type_action_id;
	uint32_t pi_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pi_type_action, &pi_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, pi_type_action_id, &pi_type_result_id
		) != 0 || action_results[pi_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 88;
	}
	uint32_t function_binding = prototype_term_new_binding(&term_db);
	uint32_t function_context;
	uint32_t function_context_certificate;
	if (function_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			function_binding,
			bool_to_pure_bool,
			PROTOTYPE_INVALID_ID,
			&function_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			function_context,
			pi_type_in_bool_context_claim,
			&function_context_certificate
		) != 0) {
		return 89;
	}
	uint32_t function_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			function_context,
			pi_type_action_id,
			&function_bridge
		) != 0) {
		return 90;
	}
	uint32_t function_projection;
	uint32_t function_variable;
	uint32_t function_variable_claim;
	uint32_t bool_variable_in_function_context_claim;
	uint32_t bool_type_in_function_context_claim;
	uint32_t pi_type_in_function_context_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, function_context, &function_projection
		) != 0 || prototype_term_var(
			&term_db, function_binding, &function_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			function_context,
			function_binding,
			bool_to_pure_bool,
			&function_variable_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_variable_claim,
			function_projection,
			&bool_variable_in_function_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_type_in_context_claim,
			function_projection,
			&bool_type_in_function_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pi_type_in_bool_context_claim,
			function_projection,
			&pi_type_in_function_context_claim
		) != 0) {
		return 91;
	}
	uint32_t applied_function;
	if (prototype_term_app(
			&term_db, function_variable, bool_variable, &applied_function
		) != 0) {
		return 92;
	}
	uint32_t app_operation = add_operation(
		&operation_graph,
		function_context,
		applied_function,
		pure_comp,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t app_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		app_operation,
		function_context,
		app_operation,
		applied_function,
		pure_comp
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
			.conclusion_claim_id = app_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = function_variable_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_variable_in_function_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	uint32_t function_to_empty;
	uint32_t pure_comp_in_function_context_claim;
	if (prototype_substitution_empty(
			&substitution_db, &context_db, function_context, &function_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_comp_is_type,
			function_to_empty,
			&pure_comp_in_function_context_claim
		) != 0) {
		return 93;
	}
	struct prototype_hott_action_request app_result_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pure_comp_in_function_context_claim,
			.source_bridge_id = function_bridge
		}
	};
	uint32_t app_result_type_action_id;
	uint32_t app_result_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, app_result_type_action,
			&app_result_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, app_result_type_action_id,
			&app_result_type_result_id
		) != 0 || action_results[app_result_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 94;
	}
	struct prototype_hott_action_request app_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = app_claim,
			.source_bridge_id = function_bridge,
			.relation_type_action_request_id = app_result_type_action_id
		}
	};
	uint32_t app_action_id;
	uint32_t app_result_id;
	int app_action_intern_status = prototype_hott_action_request_intern(
		&action_db, &kernel_view, &bridge_db, app_action, &app_action_id
	);
	int app_action_status = app_action_intern_status == 0 ?
		prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, app_action_id, &app_result_id
		) : -1;
	if (app_action_intern_status != 0 || app_action_status != 0 ||
		action_results[app_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 95;
	}
	uint32_t app_witness_claim = action_certificates[
		action_results[app_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_app_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == app_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS &&
			judgement.derivations[i].premise_count == 5) {
			found_app_witness = 1;
			break;
		}
	}
	if (!found_app_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 96;
	}
	(void)function_context_certificate;
	(void)bool_type_in_function_context_claim;
	(void)pi_type_in_function_context_claim;
	uint32_t pure_bool_computation;
	uint32_t bool_to_pure_bool_computation;
	if (prototype_term_computation_type(
			&term_db, empty_row, bool_view, &pure_bool_computation
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			pure_bool_computation,
			&bool_to_pure_bool_computation
		) != 0) {
		return 97;
	}
	uint32_t lambda_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		bool_to_pure_bool_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		bool_to_pure_bool_computation,
		universe
	);
	struct prototype_hott_action_request lambda_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = lambda_type_in_bool_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t lambda_type_action_id;
	uint32_t lambda_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, lambda_type_action,
			&lambda_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, lambda_type_action_id,
			&lambda_type_result_id
		) != 0 || action_results[lambda_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 98;
	}
	uint32_t lambda_binding = prototype_term_new_binding(&term_db);
	uint32_t lambda_body_context;
	uint32_t lambda_body_context_certificate;
	if (lambda_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			lambda_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&lambda_body_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			lambda_body_context,
			bool_type_in_context_claim,
			&lambda_body_context_certificate
		) != 0) {
		return 99;
	}
	uint32_t lambda_body_projection;
	uint32_t lambda_binder_variable;
	uint32_t lambda_binder_claim;
	uint32_t outer_bool_in_lambda_body_claim;
	uint32_t bool_type_in_lambda_body_claim;
	uint32_t pure_bool_type_in_lambda_body_claim;
	uint32_t pure_bool_type_in_bool_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_bool_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_bool_computation,
		universe
	);
	if (prototype_substitution_projection(
			&substitution_db,
			&context_db,
			lambda_body_context,
			&lambda_body_projection
		) != 0 || prototype_term_var(
			&term_db, lambda_binding, &lambda_binder_variable
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			lambda_body_context,
			lambda_binding,
			bool_view,
			&lambda_binder_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_variable_claim,
			lambda_body_projection,
			&outer_bool_in_lambda_body_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_type_in_context_claim,
			lambda_body_projection,
			&bool_type_in_lambda_body_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			pure_bool_type_in_bool_context_claim,
			lambda_body_projection,
			&pure_bool_type_in_lambda_body_claim
		) != 0) {
		return 100;
	}
	uint32_t returned_outer_bool;
	if (prototype_term_return(
			&term_db, bool_variable, &returned_outer_bool
		) != 0) {
		return 101;
	}
	uint32_t returned_outer_bool_operation = add_operation(
		&operation_graph,
		lambda_body_context,
		returned_outer_bool,
		pure_bool_computation,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t returned_outer_bool_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		returned_outer_bool_operation,
		lambda_body_context,
		returned_outer_bool_operation,
		returned_outer_bool,
		pure_bool_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_outer_bool_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = outer_bool_in_lambda_body_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
		};
	uint32_t dependent_lambda;
	if (prototype_term_lambda(
			&term_db, lambda_binding, returned_outer_bool, &dependent_lambda
		) != 0) {
		return 102;
	}
	uint32_t lambda_operation = add_operation(
		&operation_graph,
		bool_context,
		dependent_lambda,
		bool_to_pure_bool_computation,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t lambda_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		lambda_operation,
		bool_context,
		lambda_operation,
		dependent_lambda,
		bool_to_pure_bool_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			.conclusion_claim_id = lambda_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = lambda_binder_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = returned_outer_bool_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request lambda_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = lambda_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = lambda_type_action_id
		}
	};
	uint32_t lambda_action_id;
	uint32_t lambda_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, lambda_action, &lambda_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, lambda_action_id, &lambda_result_id
		) != 0 || action_results[lambda_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 103;
	}
	uint32_t lambda_witness_claim = action_certificates[
		action_results[lambda_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_lambda_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == lambda_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS &&
			judgement.derivations[i].premise_count == 4) {
			found_lambda_witness = 1;
			break;
		}
	}
	if (!found_lambda_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 104;
	}
	(void)lambda_body_context_certificate;
	(void)bool_type_in_lambda_body_claim;
	(void)pure_bool_type_in_lambda_body_claim;
	uint32_t bool_true_in_context_claim;
	uint32_t bool_false_in_context_claim;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			true_claim,
			bool_projection,
			&bool_true_in_context_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			false_claim,
			bool_projection,
			&bool_false_in_context_claim
		) != 0) {
		return 105;
	}
	struct prototype_match_case_input value_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_true
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_false
		}
	};
	uint32_t value_match;
	uint32_t motive_binding = prototype_term_new_binding(&term_db);
	uint32_t motive_variable;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_term_match(
			&term_db, bool_variable, value_match_cases, 2, &value_match
		) != 0 || prototype_term_var(
			&term_db, motive_binding, &motive_variable
		) != 0) {
		return 106;
	}
	struct prototype_match_case_input motive_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_view
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = bool_view,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = bool_view
		}
	};
	uint32_t motive_match;
	uint32_t motive_lambda;
	uint32_t value_match_classifier;
	if (prototype_term_match(
			&term_db, motive_variable, motive_cases, 2, &motive_match
		) != 0 || prototype_term_lambda(
			&term_db, motive_binding, motive_match, &motive_lambda
		) != 0 || prototype_term_app(
			&term_db, motive_lambda, bool_variable, &value_match_classifier
		) != 0) {
		return 107;
	}
	uint32_t value_match_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		value_match_classifier,
		bool_context,
		PROTOTYPE_INVALID_ID,
		value_match_classifier,
		universe
	);
	uint32_t value_match_operation = add_operation(
		&operation_graph,
		bool_context,
		value_match,
		value_match_classifier,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t value_match_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		value_match_operation,
		bool_context,
		value_match_operation,
		value_match,
		value_match_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = value_match_claim,
			.premise_count = 3,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = value_match_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_true_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = bool_false_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request value_match_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = value_match_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t value_match_type_action_id;
	uint32_t value_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, value_match_type_action,
			&value_match_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, value_match_type_action_id,
			&value_match_type_result_id
		) != 0 || action_results[value_match_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 108;
	}
	struct prototype_hott_action_request value_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = value_match_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = value_match_type_action_id
		}
	};
	uint32_t value_match_action_id;
	uint32_t value_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, value_match_action,
			&value_match_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, value_match_action_id,
			&value_match_result_id
		) != 0 || action_results[value_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 109;
	}
	uint32_t value_match_witness_claim = action_certificates[
		action_results[value_match_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_match_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				value_match_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS &&
			judgement.derivations[i].premise_count == 7) {
			found_match_witness = 1;
			break;
		}
	}
	if (!found_match_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 110;
	}
