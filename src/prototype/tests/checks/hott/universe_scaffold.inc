	indexed_match_operations[indexed_false_body_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_VAR;
	indexed_match_operations[indexed_true_body_occurrence].tag =
		PROTOTYPE_TYPED_OCCURRENCE_VAR;
	indexed_match_operations[indexed_false_return_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_RETURN;
	indexed_match_operations[indexed_true_return_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_RETURN;
	indexed_match_operations[indexed_match_operation].tag =
		PROTOTYPE_TYPED_OCCURRENCE_MATCH;
	indexed_match_operations[indexed_match_operation].first_case = 0;
	indexed_match_operations[indexed_match_operation].case_count = 2;
	indexed_match_operation_cases[0] =
		(struct prototype_typed_occurrence_match_case) {
			.context_id = indexed_proof_context,
			.refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING,
			.refinement_substitution = PROTOTYPE_INVALID_ID,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 0,
			.case_label_symbol_id = -1,
			.binder_count = 0
		};
	indexed_match_operation_cases[1] =
		(struct prototype_typed_occurrence_match_case) {
			.context_id = indexed_proof_context,
			.refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING,
			.refinement_substitution = PROTOTYPE_INVALID_ID,
			.constructor_owner = indexed_relation_type,
			.constructor_id = 1,
			.case_label_symbol_id = -1,
			.binder_count = 0
		};
	indexed_match_operation_graph.case_count = 2;
	if (fixture_add_occurrence_edge(
			&indexed_match_operation_graph,
			indexed_false_return_operation,
			PROTOTYPE_TERM_CHILD_RETURN_VALUE,
			0,
			indexed_false_body_occurrence
		) != 0 || fixture_add_occurrence_edge(
			&indexed_match_operation_graph,
			indexed_true_return_operation,
			PROTOTYPE_TERM_CHILD_RETURN_VALUE,
			0,
			indexed_true_body_occurrence
		) != 0 || fixture_add_occurrence_edge(
			&indexed_match_operation_graph,
			indexed_match_operation,
			PROTOTYPE_TERM_CHILD_SCRUTINEE,
			0,
			indexed_scrutinee_operation
		) != 0 || fixture_add_occurrence_edge(
			&indexed_match_operation_graph,
			indexed_match_operation,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
			0,
			indexed_false_return_operation
		) != 0 || fixture_add_occurrence_edge(
			&indexed_match_operation_graph,
			indexed_match_operation,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
			1,
			indexed_true_return_operation
		) != 0) {
		return 143;
	}
	struct prototype_usage_entry indexed_match_resource_usage[256];
	prototype_judgement_db_set_resource_usage_storage(
		&indexed_match_judgement,
		indexed_match_resource_usage,
		256
	);
	struct prototype_occurrence_usage_solution indexed_usage_solutions[6];
	struct prototype_usage_entry indexed_usage_entries[64];
	size_t indexed_usage_entry_count;
	if (prototype_occurrence_usage_solve(
			&indexed_match_operation_graph,
			&term_db,
			NULL,
			indexed_usage_solutions,
			6,
			indexed_usage_entries,
			64,
			&indexed_usage_entry_count
		) != 0) {
		return 143;
	}
	struct fixture_occurrence_usage_provider_context indexed_usage_provider = {
		.solutions = indexed_usage_solutions,
		.solution_count = indexed_match_operation_graph.occurrence_count,
		.entries = indexed_usage_entries,
		.entry_count = indexed_usage_entry_count
	};
	struct prototype_judgement_proposition indexed_delta_propositions[64];
	struct prototype_judgement_derivation_candidate indexed_delta_candidates[64];
	struct prototype_judgement_candidate_premise indexed_delta_premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result indexed_delta_motives[64];
	struct prototype_judgement_computation_constraint indexed_delta_constraints[64];
	struct prototype_judgement_effect_row_constraint indexed_delta_effect_rows[64];
	struct prototype_judgement_delta indexed_delta;
	prototype_judgement_delta_init(
		&indexed_delta,
		&indexed_match_judgement,
		indexed_delta_propositions,
		indexed_delta_candidates,
		64,
		indexed_delta_premises,
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		indexed_delta_motives,
		64,
		indexed_delta_constraints,
		64,
		indexed_delta_effect_rows,
		64
	);
	prototype_judgement_delta_set_context_store(
		&indexed_delta, &context_db, &substitution_db
	);
	prototype_judgement_delta_set_occurrence_store(
		&indexed_delta,
		indexed_match_operation_graph.occurrences,
		indexed_match_operation_graph.occurrence_count,
		indexed_match_operation_graph.edges,
		indexed_match_operation_graph.edge_count,
		indexed_match_operation_graph.cases,
		indexed_match_operation_graph.case_count
	);
	prototype_judgement_delta_set_occurrence_usage_provider(
		&indexed_delta,
		&indexed_usage_provider,
		fixture_occurrence_usage_provider
	);
	prototype_judgement_delta_set_context(
		&indexed_delta, indexed_proof_context
	);
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_scrutinee_operation
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_proof_binding,
			indexed_relation_type
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_false_body_occurrence
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_left_binding,
			bool_view
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_true_body_occurrence
	);
	if (prototype_judgement_delta_record_context_binding_assumption(
			&indexed_delta,
			&term_db,
			indexed_left_binding,
			bool_view
		) != 0) {
		return 143;
	}
	struct prototype_judgement_selected_evidence indexed_branch_evidence;
	if (prototype_judgement_selected_evidence_from_claim(
			&indexed_match_judgement,
			indexed_left_assumption_claim,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_false_return_operation
	);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_false_return,
			indexed_false_body_occurrence,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_true_return_operation
	);
	if (prototype_judgement_delta_record_cbpv_boundary(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_true_return,
			indexed_true_body_occurrence,
			&indexed_branch_evidence
		) != 0) {
		return 143;
	}
	prototype_judgement_delta_set_occurrence(
		&indexed_delta, indexed_match_operation
	);
	uint32_t indexed_match_classifier;
	if (prototype_judgement_delta_type_match_from_cases(
			&indexed_delta,
			&term_db,
			&type_db,
			indexed_match_term,
			term_db.terms[universe].as.universe_var.level_var,
			&indexed_match_classifier
		) != 0) {
		return 143;
	}
	indexed_match_operations[indexed_match_operation].classifier =
		indexed_match_classifier;
	indexed_match_operations[indexed_match_operation].classifier_status =
		PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED;
	if (prototype_judgement_delta_commit(&indexed_delta, 0) != 0 ||
		prototype_judgement_publish_candidates(
			&indexed_match_operation_graph, &indexed_match_judgement
		) != 0 || prototype_typed_occurrence_graph_seal(
			&indexed_match_operation_graph, &term_db, &context_db
		) != 0 || prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			prototype_default_intrinsic_environment(),
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) != 0) {
		return 143;
	}
	struct prototype_judgement_derivation* indexed_match_derivation = NULL;
	for (uint32_t i = 0; i < indexed_match_judgement.derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&indexed_match_judgement.derivations[i];
		const struct prototype_judgement_proposition* conclusion =
			derivation->conclusion_claim_id < indexed_match_judgement.claim_count ?
			prototype_judgement_claim_proposition(
				&indexed_match_judgement, derivation->conclusion_claim_id
			) : NULL;
		if (conclusion && derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM &&
			conclusion->occurrence_id == indexed_match_operation) {
			indexed_match_derivation = derivation;
			break;
		}
	}
	if (!indexed_match_derivation || indexed_match_derivation->premise_count != 3 ||
		indexed_match_derivation->premises[1].semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		indexed_match_derivation->premises[2].semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		indexed_match_derivation->premises[1].semantic_action_id ==
			indexed_match_derivation->premises[2].semantic_action_id) {
		return 144;
	}
	uint32_t saved_false_refinement =
		indexed_match_derivation->premises[1].semantic_action_id;
	indexed_match_derivation->premises[1].semantic_action_id =
		indexed_match_derivation->premises[2].semantic_action_id;
	if (prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			prototype_default_intrinsic_environment(),
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) == 0) {
		return 144;
	}
	indexed_match_derivation->premises[1].semantic_action_id =
		saved_false_refinement;
	if (prototype_judgement_validate_accepted_graph(
			&term_db,
			&type_db,
			prototype_default_intrinsic_environment(),
			&context_db,
			&substitution_db,
			&bridge_dimension_operator_db,
			&indexed_match_operation_graph,
			&indexed_match_judgement
		) != 0) {
		return 144;
	}
	(void)indexed_match_bool_type_in_context_claim;
	struct prototype_hott_action_request bool_term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = false_claim,
			.source_bridge_id = bridge,
			.relation_type_action_request_id = bool_type_action_id
		}
	};
	uint32_t bool_term_action_id;
	uint32_t bool_term_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_term_action, &bool_term_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, bool_term_action_id,
			&bool_term_result_id
		) != 0 || action_results[bool_term_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		fprintf(
			stderr,
			"bool term action failed request=%u result=%u state=%d reason=%d "
			"validate=%d source_claim=%u rank=%u\n",
			bool_term_action_id,
			bool_term_result_id,
			bool_term_result_id < action_db.result_count ?
				action_results[bool_term_result_id].outcome.state : -1,
			bool_term_result_id < action_db.result_count ?
				action_results[bool_term_result_id].outcome.residual_reason : -1,
			prototype_hott_action_db_validate(
				&action_db, &kernel_view, &bridge_db
			),
			false_claim,
			false_claim < judgement.claim_count ?
				judgement.claims[false_claim].closure_rank : PROTOTYPE_INVALID_ID
		);
		return 53;
	}
	uint32_t bool_term_certificate_id =
		action_results[bool_term_result_id].certificate_id;
	uint32_t saved_witness =
		action_certificates[bool_term_certificate_id].data.term.witness_term_id;
	action_certificates[bool_term_certificate_id].data.term.witness_term_id =
		bool_false;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 54;
	}
	action_certificates[bool_term_certificate_id].data.term.witness_term_id =
		saved_witness;
	uint32_t bool_binding = prototype_term_new_binding(&term_db);
	uint32_t bool_context;
	uint32_t bool_context_certificate;
	if (bool_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db,
			empty,
			bool_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&bool_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			bool_context,
			bool_is_type,
			&bool_context_certificate
		) != 0) {
		return 42;
	}
	uint32_t bool_bridge;
	size_t bridge_count_before = bridge_db.bridge_count;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			bool_context,
			bool_type_action_id,
			&bool_bridge
		) != 0 || bridge_db.bridge_count != bridge_count_before + 1 ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 43;
	}
	if (bool_bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bool_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) {
		return 247;
	}
	int saved_bridge_semantics = bridge_db.certificates[bool_bridge].semantics;
	bridge_db.certificates[bool_bridge].semantics =
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 247;
	}
	bridge_db.certificates[bool_bridge].semantics = saved_bridge_semantics;
	if (prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 247;
	}
	uint32_t repeated_bool_bridge;
	if (prototype_hott_bridge_db_construct_extension(
			&bridge_db, &kernel_builder, &action_db,
			bool_context,
			bool_type_action_id,
			&repeated_bool_bridge
		) != 0 || repeated_bool_bridge != bool_bridge ||
		bridge_db.bridge_count != bridge_count_before + 1) {
		return 44;
	}
	uint32_t bool_identity_bridge;
	uint32_t repeated_bool_identity_bridge;
	size_t identity_bridge_count_before = bridge_db.bridge_count;
	if (prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_context,
			bool_identity_computation_id,
			&bool_identity_bridge
		) != 0 || bool_identity_bridge == bool_bridge ||
		bridge_db.bridge_count != identity_bridge_count_before + 1 ||
		bool_identity_bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bool_identity_bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0 || prototype_hott_bridge_db_construct_identity_extension(
			&bridge_db,
			&kernel_builder,
			&action_db,
			bool_context,
			bool_identity_computation_id,
			&repeated_bool_identity_bridge
		) != 0 || repeated_bool_identity_bridge != bool_identity_bridge ||
		bridge_db.bridge_count != identity_bridge_count_before + 1) {
		return 248;
	}
	uint32_t saved_identity_fiber_certificate =
		bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id;
	bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id =
		bridge_db.certificates[bool_bridge].fiber_action_certificate_id;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 249;
	}
	bridge_db.certificates[bool_identity_bridge].fiber_action_certificate_id =
		saved_identity_fiber_certificate;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 250;
	}
	uint32_t bool_type_result_id;
	if (prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id,
			&bool_type_result_id
		) != 0 || bool_type_result_id != result_id) {
		return 45;
	}
	uint32_t type_certificate_id = action_results[result_id].certificate_id;
	if (action_certificates[type_certificate_id].data.relation_type.
			relation_family_semantics !=
		PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION) {
		return 142;
	}
	action_certificates[type_certificate_id].data.relation_type.
		relation_family_semantics = PROTOTYPE_HOTT_RELATION_FAMILY_INVALID;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 143;
	}
	action_certificates[type_certificate_id].data.relation_type.
		relation_family_semantics =
			PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION;
	uint32_t saved_relation_type =
		action_certificates[type_certificate_id].data.relation_type.relation_type_term_id;
	action_certificates[type_certificate_id].data.relation_type.relation_type_term_id = text;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 46;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_type_term_id =
		saved_relation_type;
	uint32_t relation_family = action_certificates[
		type_certificate_id
	].data.relation_type.relation_family_term_id;
	uint32_t family_left_binding;
	uint32_t inner_relation_family;
	uint32_t family_right_binding;
	uint32_t family_relation_body;
	int family_body_matches;
	if (prototype_term_pure_family_parts(
			&term_db,
			relation_family,
			&family_left_binding,
			&inner_relation_family
		) != 0 || prototype_term_pure_family_parts(
			&term_db,
			inner_relation_family,
			&family_right_binding,
			&family_relation_body
		) != 0 || prototype_term_core_shape_equal_under_binders(
			&term_db,
			(uint32_t[]) { family_left_binding, family_right_binding },
			(uint32_t[]) {
				action_certificates[type_certificate_id].data.relation_type.
					left_endpoint_binding_id,
				action_certificates[type_certificate_id].data.relation_type.
					right_endpoint_binding_id
			},
			2,
			family_relation_body,
			saved_relation_type,
			&family_body_matches
		) != 0 || !family_body_matches) {
		return 140;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_family_term_id =
		text;
	if (prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) == 0) {
		return 141;
	}
	action_certificates[type_certificate_id].data.relation_type.relation_family_term_id =
		relation_family;
	if (prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, type_action_id, &result_id
		) != 0 || action_results[result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED ||
		action_results[result_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE) {
		return 47;
	}
	uint32_t identity_action_result;
	if (prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, identity_substitution_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&identity_action_result
		) != 0 || action_results[identity_action_result].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 48;
	}
	uint32_t bool_projection;
	uint32_t bool_projection_certificate;
	if (prototype_substitution_projection(
			&substitution_db, &context_db, bool_context, &bool_projection
		) != 0 || prototype_cwf_certificate_db_certify_substitution_root(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			bool_projection,
			&bool_projection_certificate
		) != 0) {
		return 49;
	}
	struct prototype_hott_action_request projection_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source = {
				.substitution_id = bool_projection,
				.certificate_id = bool_projection_certificate
			},
			.source_bridge_id = bool_bridge,
			.target_bridge_id = bridge
		}
	};
	uint32_t projection_action_id;
	uint32_t projection_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, projection_action,
			&projection_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, projection_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&projection_result_id
		) != 0 || action_results[projection_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 50;
	}
	uint32_t bool_identity;
	uint32_t composed_projection;
	uint32_t composed_projection_certificate;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, bool_context, &bool_identity
		) != 0 || prototype_substitution_compose(
			&substitution_db,
			&context_db,
			bool_projection,
			bool_identity,
			&composed_projection
		) != 0 || prototype_cwf_certificate_db_certify_substitution_root(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			composed_projection,
			&composed_projection_certificate
		) != 0) {
		return 51;
	}
	struct prototype_hott_action_request composition_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source = {
				.substitution_id = composed_projection,
				.certificate_id = composed_projection_certificate
			},
			.source_bridge_id = bool_bridge,
			.target_bridge_id = bridge
		}
	};
	uint32_t composition_action_id;
	uint32_t composition_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, composition_action,
			&composition_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, composition_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&composition_result_id
		) != 0 || action_results[composition_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 52;
	}
	uint32_t empty_to_empty;
	uint32_t false_extension;
	uint32_t false_extension_certificate;
	int false_empty_status = prototype_substitution_empty(
			&substitution_db, &context_db, empty, &empty_to_empty
		);
	int false_extend_status = false_empty_status == 0 ? prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
				bool_false,
				bool_view,
				&false_extension
			) : -1;
	int false_coherence_status = false_extend_status == 0 ?
		prototype_substitution_db_validate_classifier_coherence(
				&substitution_db, &context_db, &term_db, &type_db
			) : -1;
	int false_root_status = false_coherence_status == 0 ?
		prototype_cwf_certificate_db_certify_substitution_root(
				&cwf_certificate_db,
				&substitution_db,
				&judgement,
				false_extension,
				&false_extension_certificate
			) : -1;
	int false_wrong_claim_status = false_coherence_status == 0 ?
		prototype_cwf_certificate_db_add_substitution(
				&cwf_certificate_db,
				&substitution_db,
				&judgement,
				false_extension,
				bool_is_type,
				&false_extension_certificate
			) : -1;
	int false_claim_status = false_coherence_status == 0 ?
		prototype_cwf_certificate_db_add_substitution(
				&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_claim,
				&false_extension_certificate
			) : -1;
	if (false_empty_status != 0 || false_extend_status != 0 ||
		false_coherence_status != 0 || false_root_status == 0 ||
		false_wrong_claim_status == 0 || false_claim_status != 0) {
		fprintf(stderr,
			"HOTT false substitution certificate failed empty=%d extend=%d "
			"coherence=%d root=%d wrong-claim=%d claim=%d\n",
			false_empty_status, false_extend_status, false_coherence_status,
			false_root_status, false_wrong_claim_status, false_claim_status);
		return 55;
	}
	struct prototype_certified_substitution_ref false_extension_ref;
	if (prototype_cwf_certified_substitution_ref_get(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_extension_certificate,
			&false_extension_ref
		) != 0 || false_extension_ref.substitution_id != false_extension) {
		return 146;
	}
	uint32_t ignored_substitution;
	if (prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			PROTOTYPE_INVALID_ID,
			bool_context,
			bool_false,
			bool_view,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_PREFIX ||
		prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			(uint32_t)term_db.term_count,
			bool_view,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_TERM_OUT_OF_RANGE ||
		prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			bool_false,
			(uint32_t)term_db.term_count,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_OUT_OF_RANGE ||
		prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			empty,
			bool_false,
			bool_view,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_TARGET ||
		prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			bool_identity,
			bool_context,
			bool_false,
			bool_view,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_TARGET_PARENT_MISMATCH ||
		prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			empty_to_empty,
			bool_context,
			bool_false,
			universe,
			&ignored_substitution
		) != PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH ||
		strcmp(prototype_substitution_extend_result_name(
			PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH
		), "extension classifier mismatch") != 0) {
		return 147;
	}
	uint32_t false_proposition_id = judgement.claims[
		false_claim
	].proposition_id;
	if (false_proposition_id >= judgement.proposition_count) {
		return 148;
	}
	struct prototype_judgement_proposition saved_false_proposition =
		judgement.propositions[false_proposition_id];
	struct prototype_certified_substitution_ref rejected_ref;
	if (saved_false_proposition.context_id != empty ||
		saved_false_proposition.subject != bool_false ||
		saved_false_proposition.classifier != bool_view ||
		prototype_cwf_certified_substitution_ref_get(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			bool_projection,
			false_extension_certificate,
			&rejected_ref
		) == 0) {
		return 148;
	}
	judgement.propositions[false_proposition_id].context_id = bool_context;
	if (prototype_cwf_certified_substitution_ref_get(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_extension_certificate,
			&rejected_ref
		) == 0) {
		return 148;
	}
	judgement.propositions[false_proposition_id] = saved_false_proposition;
	judgement.propositions[false_proposition_id].subject = universe;
	if (prototype_cwf_certified_substitution_ref_get(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_extension_certificate,
			&rejected_ref
		) == 0) {
		return 148;
	}
	judgement.propositions[false_proposition_id] = saved_false_proposition;
	judgement.propositions[false_proposition_id].classifier = universe;
	if (prototype_cwf_certified_substitution_ref_get(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			false_extension,
			false_extension_certificate,
			&rejected_ref
		) == 0) {
		return 148;
	}
	judgement.propositions[false_proposition_id] = saved_false_proposition;
	if (prototype_cwf_certificate_db_validate_substitutions(
			&cwf_certificate_db, &substitution_db, &judgement
		) != 0) {
		return 148;
	}

	uint32_t covered_composition;
	uint32_t covered_composition_certificate;
	uint32_t coverage_second_bool_binding = prototype_term_new_binding(&term_db);
	uint32_t coverage_second_bool_context;
	uint32_t uncovered_extension;
	uint32_t uncovered_composition;
	uint32_t uncovered_extension_certificate;
	uint32_t uncovered_composition_certificate;
	if (prototype_substitution_compose(
			&substitution_db,
			&context_db,
			false_extension,
			empty_to_empty,
			&covered_composition
		) != 0 || prototype_cwf_certificate_db_certify_substitution_root(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			covered_composition,
			&covered_composition_certificate
		) != 0 || coverage_second_bool_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&context_db,
			bool_context,
			coverage_second_bool_binding,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&coverage_second_bool_context
		) != 0 || prototype_substitution_extend(
			&substitution_db,
			&context_db,
			&term_db,
			&type_db,
			false_extension,
			coverage_second_bool_context,
			bool_false,
			bool_view,
			&uncovered_extension
		) != PROTOTYPE_SUBSTITUTION_EXTEND_OK ||
		prototype_substitution_compose(
			&substitution_db,
			&context_db,
			uncovered_extension,
			empty_to_empty,
			&uncovered_composition
		) != 0 || prototype_cwf_certificate_db_certify_substitution_root(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			uncovered_composition,
			&uncovered_composition_certificate
		) == 0 || prototype_cwf_certificate_db_add_substitution(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			uncovered_extension,
			false_claim,
			&uncovered_extension_certificate
		) != 0 || prototype_cwf_certificate_db_certify_substitution_root(
			&cwf_certificate_db,
			&substitution_db,
			&judgement,
			uncovered_composition,
			&uncovered_composition_certificate
		) != 0) {
		return 149;
	}
	struct prototype_hott_action_request extension_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source = {
				.substitution_id = false_extension,
				.certificate_id = false_extension_certificate
			},
			.source_bridge_id = bridge,
			.target_bridge_id = bool_bridge
		}
	};
	uint32_t extension_action_id;
	uint32_t extension_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, extension_action, &extension_action_id
		) != 0 || prototype_hott_execute_substitution_action(
			&action_db, &kernel_builder, &bridge_db, extension_action_id,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 64,
			&extension_result_id
		) != 0 || action_results[extension_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 56;
	}

	uint32_t bool_type_in_context_claim;
	uint32_t bool_variable_claim;
	uint32_t bool_variable;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_is_type,
			bool_projection,
			&bool_type_in_context_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			bool_context,
			bool_binding,
			bool_view,
			&bool_variable_claim
		) != 0 || prototype_term_var(
			&term_db, bool_binding, &bool_variable
		) != 0) {
		return 57;
	}
	struct prototype_hott_action_request contextual_bool_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t contextual_bool_type_action_id;
	uint32_t contextual_bool_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, contextual_bool_type_action,
			&contextual_bool_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, contextual_bool_type_action_id,
			&contextual_bool_type_result_id
		) != 0 || action_results[contextual_bool_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 58;
	}
	struct prototype_hott_action_request bool_variable_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = bool_variable_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = contextual_bool_type_action_id
		}
	};
	uint32_t bool_variable_action_id;
	uint32_t bool_variable_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_variable_action,
			&bool_variable_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, bool_variable_action_id,
			&bool_variable_result_id
		) != 0 || action_results[bool_variable_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 59;
	}
	const struct prototype_context* bool_relation_context =
		prototype_context_get(
			&context_db, bridges[bool_bridge].bridge_context_id
		);
	uint32_t bool_relation_variable;
	if (!bool_relation_context || prototype_term_var(
			&term_db,
			bool_relation_context->binding_id,
			&bool_relation_variable
		) != 0 || action_certificates[
			action_results[bool_variable_result_id].certificate_id
		].data.term.witness_term_id != bool_relation_variable ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 60;
	}
	uint32_t neutral_bool_identity_arguments[2] = {
		bool_variable,
		bool_variable
	};
	uint32_t neutral_bool_identity_family;
	uint32_t neutral_bool_identity_family_claim;
	uint32_t neutral_bool_identity_witness;
	uint32_t neutral_bool_identity_witness_claim;
	if (prototype_term_type_instance_make(
			&term_db,
			&type_db,
			generated_bool_identity_type,
			neutral_bool_identity_arguments,
			2,
			&neutral_bool_identity_family
		) != 0 || prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_result_id,
			bool_variable_claim,
			&neutral_bool_identity_family_claim,
			&neutral_bool_identity_witness,
			&neutral_bool_identity_witness_claim
		) != 0 || term_db.terms[neutral_bool_identity_witness].tag !=
			PROTOTYPE_TERM_MATCH || prototype_judgement_claim_proposition(
			&judgement, neutral_bool_identity_family_claim
		)->subject != neutral_bool_identity_family ||
		prototype_judgement_claim_proposition(
			&judgement, neutral_bool_identity_witness_claim
		)->classifier != neutral_bool_identity_family) {
		return 223;
	}
	uint32_t box_type_in_context_claim;
	uint32_t box_constructor;
	uint32_t boxed_bool_variable;
	if (prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			box_is_type,
			bool_projection,
			&box_type_in_context_claim
		) != 0 || prototype_term_constructor(
			&term_db, box_view, 0, &box_constructor
		) != 0 || prototype_term_app(
			&term_db, box_constructor, bool_variable, &boxed_bool_variable
		) != 0) {
		return 74;
	}
	uint32_t boxed_operation = add_operation(
		&operation_graph,
		bool_context,
		boxed_bool_variable,
		box_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t boxed_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		boxed_operation,
		bool_context,
		boxed_operation,
		boxed_bool_variable,
		box_view
	);
	if (judgement.derivation_count >= judgement.derivation_capacity) {
		return 75;
	}
	uint32_t boxed_derivation_id = (uint32_t)judgement.derivation_count++;
	struct prototype_judgement_derivation* boxed_derivation =
		&judgement.derivations[boxed_derivation_id];
	memset(boxed_derivation, 0, sizeof(*boxed_derivation));
	boxed_derivation->proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	boxed_derivation->conclusion_claim_id = boxed_claim;
	boxed_derivation->closure_rank =
		judgement.claims[bool_variable_claim].closure_rank + 1;
	judgement.claims[boxed_claim].closure_rank = boxed_derivation->closure_rank;
	memset(&boxed_derivation->rule_data, 0xff, sizeof(boxed_derivation->rule_data));
	boxed_derivation->rule_data.constructor.owner_view = box_view;
	boxed_derivation->semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	boxed_derivation->semantic_action_id = PROTOTYPE_INVALID_ID;
	boxed_derivation->premise_count = 1;
	boxed_derivation->premises = &judgement_accepted_premises[
		judgement.accepted_premise_count++
	];
	boxed_derivation->premises[0].claim_id = bool_variable_claim;
	boxed_derivation->premises[0].scoped_proposition_id = PROTOTYPE_INVALID_ID;
	uint32_t boxed_identity_family_claim;
	uint32_t boxed_identity_witness;
	uint32_t boxed_identity_witness_claim;
	if (prototype_hott_construct_degeneracy(
			&action_db,
			&kernel_builder,
			&bridge_db,
			box_identity_computation_result_id,
			boxed_claim,
			&boxed_identity_family_claim,
			&boxed_identity_witness,
			&boxed_identity_witness_claim
		) != 0 || prototype_judgement_claim_proposition(
			&judgement, boxed_identity_witness_claim
		)->classifier != prototype_judgement_claim_proposition(
			&judgement, boxed_identity_family_claim
		)->subject) {
		return 222;
	}
	struct prototype_hott_action_request box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = box_type_in_context_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t box_type_action_id;
	uint32_t box_type_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, box_type_action, &box_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, box_type_action_id, &box_type_result_id
		) != 0 || action_results[box_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 76;
	}
	struct prototype_hott_action_request boxed_term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = boxed_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = box_type_action_id
		}
	};
	uint32_t boxed_term_action_id;
	uint32_t boxed_term_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, boxed_term_action, &boxed_term_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, boxed_term_action_id,
			&boxed_term_result_id
		) != 0 || action_results[boxed_term_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 77;
	}
	uint32_t boxed_witness_claim = action_certificates[
		action_results[boxed_term_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_constructor_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id == boxed_witness_claim &&
			judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS &&
			judgement.derivations[i].premise_count == 4) {
			found_constructor_witness = 1;
			break;
		}
	}
	if (!found_constructor_witness || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 78;
	}
	uint32_t pure_box_computation;
	uint32_t returned_box;
	uint32_t pure_box_computation_claim;
	if (prototype_term_computation_type(
			&term_db, empty_row, box_view, &pure_box_computation
		) != 0 || prototype_term_return(
			&term_db, boxed_bool_variable, &returned_box
		) != 0) {
		return 79;
	}
	pure_box_computation_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_box_computation,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_box_computation,
		universe
	);
	uint32_t returned_box_operation = add_operation(
		&operation_graph,
		bool_context,
		returned_box,
		pure_box_computation,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION
	);
	uint32_t returned_box_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		returned_box_operation,
		bool_context,
		returned_box_operation,
		returned_box,
		pure_box_computation
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			.conclusion_claim_id = returned_box_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = boxed_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
		};
	struct prototype_hott_action_request pure_box_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pure_box_computation_claim,
			.source_bridge_id = bool_bridge
		}
	};
	uint32_t pure_box_type_action_id;
	uint32_t pure_box_type_result_id;
	struct prototype_hott_action_request returned_box_action;
	uint32_t returned_box_action_id;
	uint32_t returned_box_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, pure_box_type_action,
			&pure_box_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, pure_box_type_action_id,
			&pure_box_type_result_id
		) != 0 || action_results[pure_box_type_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 80;
	}
	returned_box_action = (struct prototype_hott_action_request){
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = returned_box_claim,
			.source_bridge_id = bool_bridge,
			.relation_type_action_request_id = pure_box_type_action_id
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, returned_box_action,
			&returned_box_action_id
		) != 0 || prototype_hott_execute_term_action(
			&action_db, &kernel_builder, &bridge_db, returned_box_action_id,
			&returned_box_result_id
		) != 0 || action_results[returned_box_result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return 81;
	}
	uint32_t returned_box_witness_claim = action_certificates[
		action_results[returned_box_result_id].certificate_id
	].data.term.witness_has_type_claim_id;
	int found_return_witness = 0;
	for (uint32_t i = 0; i < judgement.derivation_count; ++i) {
		if (judgement.derivations[i].conclusion_claim_id ==
				returned_box_witness_claim && judgement.derivations[i].proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS &&
			judgement.derivations[i].premise_count == 4 &&
			judgement.derivations[i].premises[3].claim_id == boxed_witness_claim) {
			found_return_witness = 1;
			break;
		}
	}
	if (!found_return_witness) {
		return 82;
	}
	uint32_t pure_box_thunk_type;
	uint32_t thunked_box;
	if (prototype_term_thunk_type(
			&term_db, pure_box_computation, &pure_box_thunk_type
		) != 0 || prototype_term_thunk(
			&term_db, returned_box, &thunked_box
		) != 0) {
		return 83;
	}
	uint32_t pure_box_thunk_type_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		pure_box_thunk_type,
		bool_context,
		PROTOTYPE_INVALID_ID,
		pure_box_thunk_type,
		universe
	);
	uint32_t thunked_box_operation = add_operation(
		&operation_graph,
		bool_context,
		thunked_box,
		pure_box_thunk_type,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t thunked_box_claim = fixture_add_claim(
		&judgement,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		thunked_box_operation,
		bool_context,
		thunked_box_operation,
		thunked_box,
		pure_box_thunk_type
	);
	judgement.derivations[judgement.derivation_count++] =
		(struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			.conclusion_claim_id = thunked_box_claim,
			.premise_count = 1,
			.premises = (struct prototype_judgement_premise_edge[]) { {
				.claim_id = returned_box_claim,
				.scoped_proposition_id = PROTOTYPE_INVALID_ID
			} }
		};
	struct prototype_hott_action_request pure_box_thunk_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = pure_box_thunk_type_claim,
			.source_bridge_id = bool_bridge
		}
	};
