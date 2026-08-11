		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		nat_classifier,
		nat_context,
		PROTOTYPE_INVALID_ID,
		nat_classifier,
		universe
	);
	uint32_t nat_ih_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		nat_ih_classifier,
		nat_succ_context,
		PROTOTYPE_INVALID_ID,
		nat_ih_classifier,
		universe
	);
	uint32_t nat_zero_in_context_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		nat_zero,
		nat_context,
		PROTOTYPE_INVALID_ID,
		nat_zero,
		nat_view
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO,
			.conclusion_claim_id = nat_zero_in_context_claim,
			.premise_count = 0
		};
	uint32_t nat_ih_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		stored_nat_succ_case->body,
		nat_succ_context,
		PROTOTYPE_INVALID_ID,
		stored_nat_succ_case->body,
		nat_ih_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
			.conclusion_claim_id = nat_ih_claim,
			.rule_data.induction.match = nat_match,
			.rule_data.induction.motive = nat_motive,
			.rule_data.induction.case_index = 1,
			.rule_data.induction.field_index = 0,
			.premise_count = 0
		};
	uint32_t nat_match_operation = add_operation(
		&operation_graph,
		nat_context,
		nat_match,
		nat_classifier,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t nat_match_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		nat_match_operation,
		nat_context,
		nat_match_operation,
		nat_match,
		nat_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = nat_match_claim,
			.premise_count = 3,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = nat_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = nat_zero_in_context_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = nat_ih_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request nat_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = nat_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t nat_type_action_id;
	uint32_t nat_type_result_id;
	uint32_t nat_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_type_action, &nat_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, nat_type_action_id, &nat_type_result_id
		) != 0 || action_results[nat_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			nat_context,
			nat_type_action_id,
			&nat_bridge
		) != 0) {
		return 130;
	}
	struct prototype_hott_action_request nat_match_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = nat_classifier_claim,
			.source_bridge_id = nat_bridge
		}
	};
	uint32_t nat_match_type_action_id;
	uint32_t nat_match_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_match_type_action,
			&nat_match_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, nat_match_type_action_id,
			&nat_match_type_result_id
		) != 0 || action_results[nat_match_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 131;
	}
	struct prototype_hott_action_request nat_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = nat_match_claim,
			.source_bridge_id = nat_bridge,
			.relation_type_action_request_id = nat_match_type_action_id
		}
	};
	uint32_t nat_match_action_id;
	uint32_t nat_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, nat_match_action, &nat_match_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, nat_match_action_id, &nat_match_result_id
		) != 0 || action_results[nat_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 132;
	}
	int found_induction_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS) {
			found_induction_witness = 1;
			break;
		}
	}
	if (!found_induction_witness) {
		return 133;
	}
	(void)nat_context_certificate;
	(void)nat_succ_context_certificate;
	(void)nat_succ_variable_claim;
	(void)nat_ih_classifier_claim;

	uint32_t second_bool_binding = prototype_term_new_binding(&term_db);
	uint32_t second_bool_context;
	uint32_t second_bool_context_certificate;
	if (second_bool_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			second_bool_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&second_bool_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			second_bool_context,
			bool_type_in_context_claim,
			&second_bool_context_certificate
		) != 0) {
		return 61;
	}
	uint32_t second_bool_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			second_bool_context,
			contextual_bool_type_action_id,
			&second_bool_bridge
		) != 0 || contexts[bridges[second_bool_bridge].bridge_context_id].depth !=
			contexts[bridges[bool_bridge].bridge_context_id].depth + 3 ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 62;
	}
	uint32_t bool_relation_type;
	uint32_t bool_relation_type_claim;
	struct prototype_hott_type_former_descriptor relation_descriptor;
	if (prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_variable,
			bool_variable,
			&bool_relation_type
		) != 0 || prototype_judgement_add_relation_type_formation(
			&judgement,
			&term_db,
			bool_context,
			bool_relation_type,
			universe,
			bool_type_in_context_claim,
			bool_type_in_context_claim,
			bool_variable_claim,
			bool_variable_claim,
			&bool_relation_type_claim
		) != 0 || prototype_hott_type_former_descriptor_query(
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_relation_type_claim,
			&relation_descriptor
		) != 0 || !relation_descriptor.admitted ||
		relation_descriptor.relation_type_action_rule !=
			PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_RELATION_HIGHER ||
		relation_descriptor.capabilities.relation_type_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.term_action !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.ordinary_reindex !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		relation_descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.transport !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.lifting !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.resource_hook !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		relation_descriptor.capabilities.artifact !=
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED ||
		(relation_descriptor.child_role_mask &
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_TERM_ACTION)) == 0) {
		return 67;
	}
	uint32_t proof_binding = prototype_term_new_binding(&term_db);
	uint32_t proof_context;
	uint32_t proof_context_certificate;
	if (proof_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			bool_context,
			proof_binding,
			bool_relation_type,
			PROTOTYPE_INVALID_ID,
			&proof_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			proof_context,
			bool_relation_type_claim,
			&proof_context_certificate
		) != 0) {
		return 68;
	}
	struct prototype_hott_action_request relation_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_relation_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t relation_type_action_id;
	uint32_t relation_type_result_id;
	uint32_t proof_bridge;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, relation_type_action,
			&relation_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, relation_type_action_id,
			&relation_type_result_id
		) != 0 || action_results[relation_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			proof_context,
			relation_type_action_id,
			&proof_bridge
		) != 0) {
		return 69;
	}
	uint32_t proof_projection;
	uint32_t relation_type_in_proof_claim;
	uint32_t proof_variable_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, proof_context, &proof_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_relation_type_claim,
			proof_projection,
			&relation_type_in_proof_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			proof_context,
			proof_binding,
			bool_relation_type,
			&proof_variable_claim
		) != 0) {
		return 70;
	}
	struct prototype_hott_action_request higher_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = relation_type_in_proof_claim,
			.source_bridge_id = proof_bridge
		}
	};
	uint32_t higher_type_action_id;
	uint32_t higher_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, higher_type_action,
			&higher_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, higher_type_action_id,
			&higher_type_result_id
		) != 0 || action_results[higher_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 71;
	}
	struct prototype_hott_action_request proof_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = proof_variable_claim,
			.source_bridge_id = proof_bridge,
			.relation_type_action_request_id = higher_type_action_id
		}
	};
	uint32_t proof_variable_action_id;
	uint32_t proof_variable_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, proof_variable_action,
			&proof_variable_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, proof_variable_action_id,
			&proof_variable_result_id
		) != 0 || action_results[proof_variable_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		terms[action_certificates[
			action_results[proof_variable_result_id].certificate_id
		].data.term.witness_term_id].tag != PROTOTYPE_TERM_VAR) {
		return 72;
	}
	uint32_t repeated_bool_extension;
	uint32_t repeated_bool_extension_certificate;
	if (prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			bool_identity,
			second_bool_context,
			bool_variable,
			bool_view,
			&repeated_bool_extension
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			repeated_bool_extension,
			bool_variable_claim,
			&repeated_bool_extension_certificate
		) != 0) {
		return 63;
	}
	struct prototype_hott_action_request repeated_bool_extension_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source = {
				.substitution_id = repeated_bool_extension,
				.certificate_id = repeated_bool_extension_certificate
			},
			.source_bridge_id = bool_bridge,
			.target_bridge_id = second_bool_bridge
		}
	};
	uint32_t repeated_bool_extension_action_id;
	uint32_t repeated_bool_extension_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, repeated_bool_extension_action,
			&repeated_bool_extension_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, repeated_bool_extension_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 64,
			&repeated_bool_extension_result_id
		) != 0 || action_results[repeated_bool_extension_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 64;
	}
	uint32_t beta_binding = prototype_term_new_binding(&term_db);
	uint32_t beta_variable;
	uint32_t beta_identity_lambda;
	uint32_t beta_false;
	uint32_t beta_false_extension;
	struct prototype_term_conversion_result exhausted_comparison;
	struct prototype_term_conversion_result completed_comparison;
	if (beta_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, beta_binding, &beta_variable
		) != 0 || prototype_term_lambda(
			&term_db, beta_binding, beta_variable, &beta_identity_lambda
		) != 0 || prototype_term_app(
			&term_db, beta_identity_lambda, bool_false, &beta_false
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			beta_false,
			bool_view,
			&beta_false_extension
		) != 0 || prototype_substitution_compare_pointwise(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			false_extension,
			beta_false_extension,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			0,
			&exhausted_comparison
		) != 0 || exhausted_comparison.status !=
			PROTOTYPE_TERM_CONVERSION_EXHAUSTED ||
		prototype_substitution_compare_pointwise(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			false_extension,
			beta_false_extension,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			8,
			&completed_comparison
		) != 0 || completed_comparison.status !=
			PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 65;
	}
	uint32_t beta_false_operation = add_operation(
		&operation_graph,
		empty,
		beta_false,
		bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t beta_false_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		beta_false_operation,
		empty,
		beta_false_operation,
		beta_false,
		bool_view
	);
	uint32_t bool_conversion_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db,
			&kernel_view,
			&bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type,
			bool_is_type,
			beta_false_claim,
			false_claim,
			bridge,
			&bool_conversion_goal
		) != 0) {
		return 148;
	}
	struct prototype_hott_relation_execution conversion_execution;
	if (prototype_parametricity_relation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_conversion_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			64,
			&conversion_execution
		) != 0 || conversion_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
		conversion_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID ||
		conversion_execution.term_action_request_id != PROTOTYPE_INVALID_ID ||
		conversion_execution.term_action_result_id != PROTOTYPE_INVALID_ID) {
		return 148;
	}
	const struct prototype_judgement_proposition* conversion_witness =
		prototype_judgement_claim_proposition(
			&judgement, conversion_execution.relation_witness_claim_id
		);
	if (!conversion_witness || conversion_witness->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_term_relation_type_info(
			&term_db,
			conversion_witness->classifier,
			&(uint32_t) { 0 },
			&(uint32_t) { 0 },
			&(uint32_t) { 0 },
			&(uint32_t) { 0 }
		) != 0) {
		return 148;
	}
	struct prototype_hott_relation_execution distinct_execution;
	if (prototype_parametricity_relation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_distinct_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32,
			&distinct_execution
		) != 0 || distinct_execution.relation_type_action_request_id == PROTOTYPE_INVALID_ID ||
		distinct_execution.relation_type_action_result_id == PROTOTYPE_INVALID_ID ||
		action_results[distinct_execution.relation_type_action_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		distinct_execution.term_action_request_id != PROTOTYPE_INVALID_ID ||
		distinct_execution.term_action_result_id != PROTOTYPE_INVALID_ID ||
		distinct_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY ||
		distinct_execution.relation_witness_claim_id != PROTOTYPE_INVALID_ID) {
		return 134;
	}
	struct prototype_hott_relation_execution diagonal_execution;
	if (prototype_parametricity_relation_plan_and_execute(
			&goal_db,
			&candidate_db,
			&work_db,
			&action_db,
			&kernel_builder,
			&bridge_db,
			NULL,
			bool_diagonal_goal,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			64,
			&diagonal_execution
		) != 0 || diagonal_execution.relation_type_action_request_id !=
			bool_type_action_id || diagonal_execution.relation_type_action_result_id !=
			bool_type_result_id || diagonal_execution.term_action_request_id !=
			bool_term_action_id || diagonal_execution.term_action_result_id !=
			bool_term_result_id || diagonal_execution.materialization_state !=
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
		diagonal_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID) {
		return 66;
	}
	uint32_t atomic_relation_type;
	uint32_t atomic_relation_type_claim;
	if (prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_false,
			bool_true,
			&atomic_relation_type
		) != 0 || prototype_judgement_add_relation_type_formation(
			&judgement,
			&term_db,
			empty,
			atomic_relation_type,
			universe,
			bool_is_type,
			bool_is_type,
			false_claim,
			true_claim,
			&atomic_relation_type_claim
		) != 0) {
		return 135;
	}
	struct prototype_hott_action_request atomic_failure_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = atomic_relation_type_claim,
			.source_bridge_id = bridge
		}
	};
	uint32_t atomic_failure_request_id;
	uint32_t ignored_result_id;
	size_t certificate_count_before_failure = action_db.certificate_count;
	size_t saved_result_capacity = action_db.result_capacity;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, atomic_failure_request,
			&atomic_failure_request_id
		) != 0) {
		return 135;
	}
	action_db.result_capacity = action_db.result_count;
	int atomic_failure_status = prototype_parametricity_execute_relation_type_action(
		&action_db, &kernel_builder, &bridge_db, atomic_failure_request_id, &ignored_result_id
	);
	action_db.result_capacity = saved_result_capacity;
	if (atomic_failure_status == 0 || action_db.certificate_count !=
		certificate_count_before_failure || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 135;
	}

	uint32_t generated_bool_identity;
	uint32_t repeated_generated_bool_identity;
	uint32_t generated_box_identity;
	if (prototype_type_declaration_origins_validate(
			&type_db, &term_db
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, bool_view, empty, &generated_bool_identity
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, bool_view, empty, &repeated_generated_bool_identity
		) != 0 || prototype_type_declaration_add_generated_identity(
			&type_db, box_view, empty, &generated_box_identity
		) != 0 || generated_bool_identity !=
			repeated_generated_bool_identity || generated_bool_identity ==
			generated_box_identity || type_db.type_declarations[
			generated_bool_identity
		].name_symbol_id != -1 || type_db.type_declarations[
			generated_bool_identity
		].origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		type_db.type_declarations[
			generated_bool_identity
		].origin_source_carrier_term_id != bool_view ||
		prototype_type_declaration_origins_validate(
			&type_db, &term_db
		) != 0) {
		return 136;
	}
	if (prototype_type_declaration_rebuild_representations(
			&term_db, &type_db, &context_db
		) != 0) {
		return 218;
	}
	if (prototype_judgement_publish_candidates(
			&operation_graph, &judgement
		) != 0) {
		return 222;
	}
	free(type_db.representations);
	return 0;
