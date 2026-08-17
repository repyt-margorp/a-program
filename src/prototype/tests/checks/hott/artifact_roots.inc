	/* Object action must reconstruct Match through ordinary kernel rules. */
	uint32_t object_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t object_motive_context;
	uint32_t object_motive_context_certificate;
	uint32_t object_motive_projection;
	uint32_t bool_has_type_in_context_claim;
	uint32_t bool_has_type_in_motive_context_claim;
	uint32_t object_motive_binder_claim;
	uint32_t object_motive;
	uint32_t object_motive_classifier;
	uint32_t object_motive_claim;
	uint32_t object_match_classifier;
	uint32_t object_match_classifier_claim;
	uint32_t object_match_classifier_is_type_claim;
	uint32_t object_match;
	uint32_t object_match_claim;
	uint32_t converted_object_match_claim;
	uint32_t object_match_branch_claims[2] = {
		bool_true_in_context_claim,
		bool_false_in_context_claim
	};
	if (object_motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			bool_projection,
			&bool_has_type_in_context_claim
		) != 0 || prototype_context_extend(
			&context_db,
			bool_context,
			object_motive_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&object_motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			object_motive_context,
			bool_type_in_context_claim,
			&object_motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			object_motive_context,
			bool_context,
			&object_motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type_in_context_claim,
			object_motive_projection,
			&bool_has_type_in_motive_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			object_motive_context,
			object_motive_binding,
			bool_view,
			&object_motive_binder_claim
		) != 0 || prototype_term_lambda(
			&term_db, object_motive_binding, bool_view, &object_motive
		) != 0 || prototype_term_pi(
			&term_db, bool_view, universe, &object_motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_context,
			object_motive,
			object_motive_classifier,
			object_motive_binder_claim,
			bool_has_type_in_motive_context_claim,
			&object_motive_claim
		) != 0 || prototype_term_app(
			&term_db,
			object_motive,
			bool_variable,
			&object_match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_context,
			object_match_classifier,
			universe,
			object_motive_claim,
			bool_variable_claim,
			&object_match_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			bool_context,
			object_match_classifier,
			universe,
			object_match_classifier_claim,
			&object_match_classifier_is_type_claim
		) != 0 || prototype_term_match(
			&term_db, bool_variable, value_match_cases, 2, &object_match
		) != 0 || prototype_judgement_add_match_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&operation_graph,
			bool_context,
			object_match,
			object_match_classifier,
			object_match_classifier_claim,
			object_match_branch_claims,
			2,
			&object_match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			&judgement,
			&term_db,
			&type_db,
			bool_context,
			object_match,
			bool_view,
			object_match_claim,
			&converted_object_match_claim
		) != 0) {
		return 279;
	}
	struct prototype_hott_action_request contextual_bool_identity = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_type_in_context_claim,
			.source_bridge_id = bool_identity_bridge
		}
	};
	uint32_t contextual_bool_identity_id;
	uint32_t contextual_bool_identity_result_id;
	struct prototype_hott_action_request object_match_action = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = converted_object_match_claim,
			.source_bridge_id = bool_identity_bridge,
			.identity_type_action_request_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t object_match_action_id;
	uint32_t object_match_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			contextual_bool_identity,
			&contextual_bool_identity_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			contextual_bool_identity_id,
			&contextual_bool_identity_result_id
		) != 0 || action_results[
			contextual_bool_identity_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 280;
	}
	object_match_action.key.object_term.identity_type_action_request_id =
		contextual_bool_identity_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			object_match_action,
			&object_match_action_id
		) != 0 || prototype_hott_execute_object_term_action(
			&action_db,
			&kernel_builder,
			&bridge_db,
			object_match_action_id,
			&object_match_result_id
		) != 0 || action_results[object_match_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || action_results[
			object_match_result_id
		].certificate_id >= action_db.certificate_count) {
		return 281;
	}
	const struct prototype_hott_object_term_action_certificate*
		object_match_certificate = &action_certificates[
			action_results[object_match_result_id].certificate_id
		].data.object_term;
	const struct prototype_judgement_proposition* object_match_witness =
		prototype_judgement_claim_proposition(
			&judgement, object_match_certificate->witness_has_type_claim_id
		);
	int found_object_match_elim = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				object_match_certificate->witness_has_type_claim_id &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
			uint32_t premise = judgement.derivations[i].premises[0].claim_id;
			for (uint32_t j = 0; j < judgement.derivation_count; ++j) {
				if (judgement.derivations[j].conclusion_claim_id == premise &&
					judgement.derivations[j].proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM) {
					found_object_match_elim = 1;
				}
			}
		}
	}
	if (!object_match_witness || object_match_witness->subject >=
			term_db.term_count || term_db.terms[object_match_witness->subject].tag !=
			PROTOTYPE_TERM_MATCH || !found_object_match_elim ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 282;
	}
	(void)object_motive_context_certificate;
	(void)object_match_classifier_is_type_claim;
	uint32_t unbox_binding = prototype_term_new_binding(&term_db);
	uint32_t unbox_variable;
	if (unbox_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, unbox_binding, &unbox_variable
		) != 0) {
		return 111;
	}
	struct prototype_case_binder unbox_binder = {
		.binding_id = unbox_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input unbox_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = box_view,
		.constructor_id = 0,
		.binders = &unbox_binder,
		.binder_count = 1,
		.body = unbox_variable
	};
	uint32_t unbox_match;
	if (prototype_term_match(
			&term_db, boxed_bool_variable, &unbox_case, 1, &unbox_match
		) != 0) {
		return 112;
	}
	const struct prototype_match_case* stored_unbox_case =
		&term_db.cases[term_db.terms[unbox_match].as.match.first_case];
	uint32_t stored_unbox_binding = term_db.case_binders[
		stored_unbox_case->first_binder
	].binding_id;
	uint32_t unbox_context;
	uint32_t unbox_context_certificate;
	if (prototype_context_extend(
			&context_db,
			bool_context,
			stored_unbox_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&unbox_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			unbox_context,
			bool_type_in_context_claim,
			&unbox_context_certificate
		) != 0) {
		return 113;
	}
	uint32_t unbox_body_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			unbox_context,
			stored_unbox_binding,
			bool_view,
			&unbox_body_claim
		) != 0) {
		return 114;
	}
	uint32_t unbox_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t unbox_motive_variable;
	uint32_t unbox_motive_case_binding = prototype_term_new_binding(&term_db);
	if (unbox_motive_binding == PROTOTYPE_INVALID_ID ||
		unbox_motive_case_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, unbox_motive_binding, &unbox_motive_variable
		) != 0) {
		return 115;
	}
	struct prototype_case_binder unbox_motive_case_binder = {
		.binding_id = unbox_motive_case_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input unbox_motive_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = box_view,
		.constructor_id = 0,
		.binders = &unbox_motive_case_binder,
		.binder_count = 1,
		.body = bool_view
	};
	uint32_t unbox_motive_match;
	uint32_t unbox_motive_lambda;
	uint32_t unbox_classifier;
	if (prototype_term_match(
			&term_db,
			unbox_motive_variable,
			&unbox_motive_case,
			1,
			&unbox_motive_match
		) != 0 || prototype_term_lambda(
			&term_db,
			unbox_motive_binding,
			unbox_motive_match,
			&unbox_motive_lambda
		) != 0 || prototype_term_app(
			&term_db,
			unbox_motive_lambda,
			boxed_bool_variable,
			&unbox_classifier
		) != 0) {
		return 116;
	}
	uint32_t unbox_classifier_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		unbox_classifier,
		bool_context,
		PROTOTYPE_INVALID_ID,
		unbox_classifier,
		universe
	);
	uint32_t unbox_operation = add_operation(
		&operation_graph,
		bool_context,
		unbox_match,
		unbox_classifier,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t unbox_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		unbox_operation,
		bool_context,
		unbox_operation,
		unbox_match,
		unbox_classifier
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			.conclusion_claim_id = unbox_claim,
			.premise_count = 2,
			.premises = (struct prototype_judgement_premise_edge[]) {
				{ .claim_id = unbox_classifier_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID },
				{ .claim_id = unbox_body_claim,
				  .scoped_proposition_id = PROTOTYPE_INVALID_ID }
			}
		};
	struct prototype_hott_action_request unbox_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = unbox_classifier_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t unbox_type_action_id;
	uint32_t unbox_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, unbox_type_action,
			&unbox_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, unbox_type_action_id,
			&unbox_type_result_id
		) != 0 || action_results[unbox_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 117;
	}
	struct prototype_hott_action_request unbox_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = unbox_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = unbox_type_action_id
		}
	};
	uint32_t unbox_action_id;
	uint32_t unbox_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, unbox_action, &unbox_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, unbox_action_id, &unbox_result_id
		) != 0 || action_results[unbox_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 118;
	}
	(void)unbox_context_certificate;
	struct prototype_hott_action_request nat_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = nat_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t nat_identity_computation_id;
	uint32_t nat_identity_computation_result_id;
	int nat_intern_status = prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			nat_identity_computation,
			&nat_identity_computation_id
		);
	int nat_execute_status = nat_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_id,
			&nat_identity_computation_result_id
		) : -1;
	if (nat_intern_status != 0 || nat_execute_status != 0 ||
		action_results[nat_identity_computation_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[nat_identity_computation_result_id].certificate_id >=
			action_db.certificate_count) {
		return 172;
	}
	const struct prototype_hott_action_certificate* nat_identity_certificate =
		&action_db.certificates[
			action_results[nat_identity_computation_result_id].certificate_id
		];
	uint32_t nat_identity_action_source;
	uint32_t nat_identity_action_operator;
	if (nat_identity_certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		prototype_term_dimension_action_info(
			&term_db,
			nat_identity_certificate->data.identity_type.
				backing_type_former_term_id,
			&nat_identity_action_source,
			&nat_identity_action_operator
		) != 0 || nat_identity_action_source != nat_view ||
		!prototype_dimension_operator_get(
			&bridge_dimension_operator_db, nat_identity_action_operator
		) ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 173;
	}
	uint32_t nat_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_variable;
	uint32_t nat_context;
	uint32_t nat_context_certificate;
	if (nat_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_binding, &nat_variable
		) != 0 || prototype_context_extend(
			&context_db,
			empty,
			nat_binding,
			nat_view,
			PROTOTYPE_INVALID_ID,
			&nat_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			nat_context,
			nat_is_type,
			&nat_context_certificate
		) != 0) {
		return 119;
	}
	uint32_t nat_variable_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			nat_context,
			nat_binding,
			nat_view,
			&nat_variable_claim
		) != 0) {
		return 120;
	}
	uint32_t nat_projection;
	uint32_t nat_type_in_context_claim;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, nat_context, &nat_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			nat_is_type,
			nat_projection,
			&nat_type_in_context_claim
		) != 0) {
		return 121;
	}
	uint32_t nat_zero;
	uint32_t nat_succ;
	if (prototype_term_constructor(
			&term_db, nat_view, 0, &nat_zero
		) != 0 || prototype_term_constructor(
			&term_db, nat_view, 1, &nat_succ
		) != 0) {
		return 122;
	}
	uint32_t nat_succ_zero;
	uint32_t nat_succ_succ_zero;
	if (prototype_term_app(
			&term_db, nat_succ, nat_zero, &nat_succ_zero
		) != 0 || prototype_term_app(
			&term_db, nat_succ, nat_succ_zero, &nat_succ_succ_zero
		) != 0) {
		return 145;
	}
	uint32_t nat_zero_claim;
	uint32_t nat_succ_zero_claim;
	uint32_t nat_succ_succ_zero_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			nat_zero,
			nat_view,
			&nat_zero_claim
		) != 0 || prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			nat_succ_zero,
			nat_view,
			(uint32_t[]) { nat_zero_claim },
			1,
			&nat_succ_zero_claim
		) != 0 || prototype_judgement_add_constructor_spine_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			nat_succ_succ_zero,
			nat_view,
			(uint32_t[]) { nat_succ_zero_claim },
			1,
			&nat_succ_succ_zero_claim
		) != 0) {
		return 242;
	}
	uint32_t nat_degeneracy_family_claim;
	uint32_t nat_degeneracy_witness;
	uint32_t nat_degeneracy_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_result_id,
			nat_succ_succ_zero_claim,
			&nat_degeneracy_family_claim,
			&nat_degeneracy_witness,
			&nat_degeneracy_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, nat_degeneracy_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, nat_degeneracy_family_claim
		)->subject || nat_degeneracy_witness >= term_db.term_count ||
		term_db.terms[nat_degeneracy_witness].tag != PROTOTYPE_TERM_APP) {
		return 223;
	}
	uint32_t nat_identity_root_id;
	uint32_t concrete_nat_identity_root_id;
	uint32_t higher_identity_root_id;
	uint32_t concrete_higher_identity_root_id;
	uint32_t neutral_declaration_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
		neutral,
		empty,
		PROTOTYPE_INVALID_ID,
		neutral,
		bool_view
	);
	if (neutral_declaration_claim == PROTOTYPE_INVALID_ID ||
		judgement.derivation_count >= judgement.derivation_capacity) {
		return 270;
	}
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation) {
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_DECLARATION,
			.conclusion_claim_id = neutral_declaration_claim,
			.closure_rank = 0,
			.rule_data.words = {
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			},
			.semantic_action_kind =
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID,
			.semantic_action_id = PROTOTYPE_INVALID_ID,
			.hash_next = PROTOTYPE_INVALID_ID
		};
	uint32_t neutral_identity_family_claim;
	uint32_t neutral_identity_witness;
	uint32_t neutral_identity_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			neutral_declaration_claim,
			&neutral_identity_family_claim,
			&neutral_identity_witness,
			&neutral_identity_witness_claim
		) != 0) {
		return 270;
	}
	uint32_t neutral_nat_identity_left;
	uint32_t neutral_nat_identity_family;
	uint32_t neutral_nat_identity_family_claim;
	uint32_t neutral_nat_identity_witness;
	uint32_t neutral_nat_identity_witness_claim;
	size_t neutral_nat_derivation_mark = judgement.derivation_count;
	if (prototype_term_app(
			&term_db,
			nat_identity_certificate->data.identity_type.backing_type_former_term_id,
			nat_variable,
			&neutral_nat_identity_left
		) != 0 || prototype_term_app(
			&term_db,
			neutral_nat_identity_left,
			nat_variable,
			&neutral_nat_identity_family
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			nat_identity_computation_result_id,
			nat_variable_claim,
			&neutral_nat_identity_family_claim,
			&neutral_nat_identity_witness,
			&neutral_nat_identity_witness_claim
		) != 0 || neutral_nat_identity_witness >= term_db.term_count ||
		term_db.terms[neutral_nat_identity_witness].tag !=
			PROTOTYPE_TERM_DIMENSION_ACTION || term_db.terms[
				neutral_nat_identity_witness
			].as.dimension_action.source != nat_variable ||
		prototype_judgement_claim_proposition(
			&judgement, neutral_nat_identity_family_claim
		)->subject != neutral_nat_identity_family || prototype_judgement_claim_proposition(
			&judgement, neutral_nat_identity_witness_claim
		)->classifier != neutral_nat_identity_family) {
		return 224;
	}
	int found_neutral_nat_action = 0;
	for (size_t i = neutral_nat_derivation_mark;
		i < judgement.derivation_count;
		++i) {
		if (judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM) {
			found_neutral_nat_action = 1;
			break;
		}
	}
	if (!found_neutral_nat_action) {
		return 225;
	}
	uint32_t neutral_identity_root_id;
	uint32_t composed_identity_root_id;
	uint32_t neutral_nat_identity_root_id;
	size_t final_identity_root_mark = identity_interface.identity_root_count;
	if (prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			nat_identity_computation_result_id,
			PROTOTYPE_INVALID_ID,
			&nat_identity_root_id
		) != 0 || nat_identity_root_id != final_identity_root_mark ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement,
			nat_is_type,
			nat_degeneracy_family_claim,
			nat_degeneracy_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&concrete_nat_identity_root_id
		) != 0 || concrete_nat_identity_root_id != final_identity_root_mark + 1 ||
		prototype_hott_register_identity_root(
			&identity_interface,
			&action_db,
			&kernel_view,
			&bridge_db,
			higher_identity_result_id,
			PROTOTYPE_INVALID_ID,
			&higher_identity_root_id
		) != 0 || higher_identity_root_id != final_identity_root_mark + 2 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement,
			false_identity_is_type,
			higher_identity_family_claim,
			higher_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION,
			&concrete_higher_identity_root_id
		) != 0 || concrete_higher_identity_root_id != final_identity_root_mark + 3 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement,
			bool_is_type,
			neutral_identity_family_claim,
			neutral_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&neutral_identity_root_id
		) != 0 || neutral_identity_root_id != final_identity_root_mark + 4 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement,
			thunk_bool_function_is_type,
			composed_identity_family_claim,
			composed_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			&composed_identity_root_id
		) != 0 || composed_identity_root_id != final_identity_root_mark + 5 ||
		prototype_artifact_interface_add_identity_root(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement,
			nat_is_type,
			neutral_nat_identity_family_claim,
			neutral_nat_identity_witness_claim,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
			&neutral_nat_identity_root_id
		) != 0 || neutral_nat_identity_root_id != final_identity_root_mark + 6) {
		return 244;
	}
	if (getenv("A_PROGRAM_HOTT_REVERSE_DERIVATIONS")) {
		for (size_t i = 0; i < judgement.derivation_count / 2; ++i) {
			size_t j = judgement.derivation_count - i - 1;
			struct prototype_judgement_derivation temporary =
				judgement.derivations[i];
			judgement.derivations[i] = judgement.derivations[j];
			judgement.derivations[j] = temporary;
		}
		if (prototype_judgement_db_rebuild_index(&judgement) != 0) {
			return 273;
		}
	}
	if (prototype_artifact_interface_validate_identity_roots(
			&identity_interface,
			&term_db,
			&type_db,
			&context_db,
			&bridge_dimension_operator_db,
			&judgement
		) != 0 || prototype_type_declaration_rebuild_representations(
			&term_db, &type_db, &context_db
		) != 0 || write_identity_root_artifact(
			"/tmp/a-program-hott-identity-root.apo",
			&identity_interface,
			&term_db,
			&type_db,
			&judgement,
			&cwf_certificate_db,
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&operation_graph
		) != 0) {
		return 244;
	}
	struct prototype_intrinsic_namespace_binding int64_bindings[32];
	struct prototype_intrinsic_environment int64_environment;
	if (fixture_int64_intrinsic_environment(
			int64_bindings,
			sizeof(int64_bindings) / sizeof(int64_bindings[0]),
			&int64_environment
		) != 0 || prototype_intrinsic_environment_fingerprint(
			&int64_environment
		) == prototype_intrinsic_environment_fingerprint(
			prototype_default_intrinsic_environment()
		) || write_identity_root_artifact_with_environment(
			"/tmp/a-program-hott-identity-root-int64.apo",
			&int64_environment,
			&identity_interface,
			&term_db,
			&type_db,
			&judgement,
			&cwf_certificate_db,
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&operation_graph
		) != 0 || fixture_artifact_interface_environment_round_trip(
			"/tmp/a-program-hott-identity-root-int64.apo",
			&int64_environment,
			prototype_default_intrinsic_environment(),
			&identity_interface
		) != 0) {
		return 274;
	}
	(void)neutral_identity_witness;
	uint32_t unused_binding = prototype_term_new_binding(&term_db);
	uint32_t unused_context;
	uint32_t unused_projection;
	uint32_t unused_relation;
	uint32_t unused_bool_projection;
	uint32_t unused_bool_is_type;
	uint32_t unused_identity_request_id;
	uint32_t unused_identity_result_id;
	struct prototype_hott_action_request unused_identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = first_box_endpoint_bridge
		}
	};
	if (unused_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			unused_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&unused_context
		) != 0 || prototype_substitution_projection(
			&substitution_db,
			&context_db,
			unused_context,
			&unused_projection
		) != 0 || prototype_term_relation_type(
			&term_db,
			bool_view,
			bool_view,
			bool_false,
			bool_true,
			&unused_relation
		) != 0) {
		return 245;
	}
	if (prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			box_endpoint_path[0],
			empty,
			&unused_bool_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			unused_bool_projection,
			&unused_bool_is_type
		) != 0) {
		return 245;
	}
	unused_identity_request.key.identity_type.source_claim_id =
		unused_bool_is_type;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			unused_identity_request,
			&unused_identity_request_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			unused_identity_request_id,
			&unused_identity_result_id
		) != 0 || action_results[unused_identity_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY || write_identity_root_artifact(
			"/tmp/a-program-hott-identity-root-perturbed.apo",
			&identity_interface,
			&term_db,
				&type_db,
					&judgement,
					&cwf_certificate_db,
					&context_db,
				&substitution_db,
				&bridge_dimension_operator_db,
				&operation_graph
		) != 0) {
		return 245;
	}
	(void)unused_projection;
	(void)unused_relation;
	uint32_t nat_constructor_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			nat_is_type, nat_is_type,
			nat_succ_zero_claim, nat_succ_succ_zero_claim,
			bridge, &nat_constructor_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, nat_constructor_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || count_rule(
			&candidate_db,
			nat_constructor_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 1) {
		return 145;
	}
	uint32_t nat_constructor_candidate = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_db.candidate_count; ++i) {
		if (candidate_db.candidates[i].conclusion_goal_id ==
				nat_constructor_goal &&
			candidate_db.candidates[i].rule ==
				PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR) {
			nat_constructor_candidate = i;
			break;
		}
	}
	if (nat_constructor_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[nat_constructor_candidate].child_edge_count != 1) {
		return 145;
	}
	const struct prototype_parametricity_child_edge* nat_field_edge =
		&candidate_db.child_edges[
			candidate_db.candidates[nat_constructor_candidate].first_child_edge
		];
	const struct prototype_parametricity_relation_goal* nat_field_goal =
		prototype_parametricity_relation_goal_db_get(
			&goal_db, nat_field_edge->child_goal_id
		);
	if (!nat_field_goal || nat_field_edge->role !=
			PROTOTYPE_HOTT_CHILD_ADT_FIELD || nat_field_edge->ordinal != 0 ||
		nat_field_goal->left_claim_id != nat_zero_claim ||
		nat_field_goal->right_claim_id != nat_succ_zero_claim ||
		nat_field_goal->left_carrier_claim_id != nat_is_type ||
		nat_field_goal->right_carrier_claim_id != nat_is_type) {
		return 145;
	}
	uint32_t nat_motive_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_motive_variable;
	uint32_t nat_motive_succ_binding = prototype_term_new_binding(&term_db);
	if (nat_motive_binding == PROTOTYPE_INVALID_ID ||
		nat_motive_succ_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_motive_binding, &nat_motive_variable
		) != 0) {
		return 123;
	}
	struct prototype_case_binder nat_motive_succ_binder = {
		.binding_id = nat_motive_succ_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input nat_motive_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = nat_view
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 1,
			.binders = &nat_motive_succ_binder,
			.binder_count = 1,
			.body = nat_view
		}
	};
	uint32_t nat_motive_match;
	uint32_t nat_motive;
	if (prototype_term_match(
			&term_db,
			nat_motive_variable,
			nat_motive_cases,
			2,
			&nat_motive_match
		) != 0 || prototype_term_lambda(
			&term_db, nat_motive_binding, nat_motive_match, &nat_motive
		) != 0) {
		return 124;
	}
	uint32_t nat_ih_scope = prototype_term_new_ih_scope(&term_db);
	uint32_t nat_succ_binding = prototype_term_new_binding(&term_db);
	uint32_t nat_succ_variable;
	uint32_t nat_ih;
	if (nat_ih_scope == PROTOTYPE_INVALID_ID ||
		nat_succ_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, nat_succ_binding, &nat_succ_variable
		) != 0 || prototype_term_induction_hypothesis(
			&term_db, nat_ih_scope, nat_succ_variable, &nat_ih
		) != 0) {
		return 125;
	}
	struct prototype_case_binder nat_succ_binder = {
		.binding_id = nat_succ_binding,
		.is_recursive = 1
	};
	struct prototype_match_case_input nat_match_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = nat_zero
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = nat_view,
			.constructor_id = 1,
			.binders = &nat_succ_binder,
			.binder_count = 1,
			.body = nat_ih
		}
	};
	uint32_t nat_match;
	if (prototype_term_match_with_ih_scope(
			&term_db,
			nat_variable,
			nat_match_cases,
			2,
			nat_ih_scope,
			&nat_match
		) != 0 || prototype_term_set_ih_scope_term(
			&term_db, nat_ih_scope, nat_match
		) != 0) {
		return 126;
	}
	const struct prototype_match_case* stored_nat_succ_case = &term_db.cases[
		term_db.terms[nat_match].as.match.first_case + 1
	];
	uint32_t stored_nat_succ_binding = term_db.case_binders[
		stored_nat_succ_case->first_binder
	].binding_id;
	uint32_t nat_succ_context;
	uint32_t nat_succ_context_certificate;
	if (prototype_context_extend(
			&context_db,
			nat_context,
			stored_nat_succ_binding,
			nat_view,
			PROTOTYPE_INVALID_ID,
			&nat_succ_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			nat_succ_context,
			nat_type_in_context_claim,
			&nat_succ_context_certificate
		) != 0) {
		return 127;
	}
	uint32_t nat_succ_variable_claim;
	if (prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			nat_succ_context,
			stored_nat_succ_binding,
			nat_view,
			&nat_succ_variable_claim
		) != 0) {
		return 128;
	}
	uint32_t nat_classifier;
	uint32_t nat_ih_classifier;
	if (prototype_term_app(
			&term_db, nat_motive, nat_variable, &nat_classifier
		) != 0 || prototype_term_app(
			&term_db,
			nat_motive,
			term_db.terms[stored_nat_succ_case->body].
				as.induction_hypothesis.argument,
			&nat_ih_classifier
		) != 0) {
		return 129;
	}
	uint32_t nat_classifier_claim = fixture_add_claim(
		&judgement,
