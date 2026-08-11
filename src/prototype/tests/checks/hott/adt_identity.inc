	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_context_db context_db;
	struct prototype_substitution_db substitution_db;
	struct prototype_judgement_db judgement;
	struct prototype_cwf_certificate_db cwf_certificate_db;
	struct prototype_operation_graph operation_graph;
	struct prototype_hott_bridge_db bridge_db;
	struct prototype_parametricity_relation_goal_db goal_db;
	struct prototype_parametricity_candidate_db candidate_db;
	struct prototype_parametricity_work_db work_db;
	struct prototype_parametricity_residual_db residual_db;
	struct prototype_hott_action_db action_db;
	struct prototype_kernel_builder kernel_builder;
	struct prototype_kernel_view kernel_view;

	prototype_term_db_init(
		&term_db, terms, TERM_CAPACITY, cases, case_labels, 256,
		case_binders, 256, ih_scopes, 64
	);
	if (getenv("A_PROGRAM_HOTT_PREALLOCATE_BINDING") &&
		prototype_term_new_binding(&term_db) == PROTOTYPE_INVALID_ID) {
		return 246;
	}
	prototype_type_declaration_db_init(
		&type_db, type_declarations, 16, constructors, 32, parameters, 8,
		field_types, 64, type_exprs, 32
	);
	prototype_context_db_init(&context_db, contexts, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitution_db, substitutions, SUBSTITUTION_CAPACITY
	);
	prototype_judgement_db_init(
		&judgement, propositions, derivation_candidates, claims,
		derivations, CLAIM_CAPACITY,
		judgement_candidate_premises,
		CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		judgement_accepted_premises,
		CLAIM_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_judgement_db_set_resource_usage_storage(
		&judgement,
		judgement_resource_usage,
		CLAIM_CAPACITY * 8
	);
	prototype_cwf_certificate_db_init(
		&cwf_certificate_db, cwf_certificates, 1024
	);
	memset(&operation_graph, 0, sizeof(operation_graph));
	operation_graph.operations = operations;
	operation_graph.operation_capacity = OPERATION_CAPACITY;
	operation_graph.cases = operation_cases;
	operation_graph.case_capacity = 128;
	fixture_set_operation_graph(&term_db, &operation_graph);
	kernel_builder = (struct prototype_kernel_builder) {
		.contexts = &context_db,
		.substitutions = &substitution_db,
		.cwf_certificates = &cwf_certificate_db,
		.terms = &term_db,
		.type_declarations = &type_db,
		.operations = &operation_graph,
		.judgement = &judgement
	};
	if (prototype_kernel_builder_view(&kernel_builder, &kernel_view) != 0) {
		return 138;
	}
	prototype_hott_bridge_db_init(
		&bridge_db, bridges, 128, bridge_certificates, 128
	);
	prototype_parametricity_relation_goal_db_init(&goal_db, goals, GOAL_CAPACITY);
	prototype_parametricity_candidate_db_init(
		&candidate_db, candidates, CANDIDATE_CAPACITY,
		claim_premises, PREMISE_CAPACITY, child_edges, PREMISE_CAPACITY,
		conversion_premises, PREMISE_CAPACITY,
		context_certificate_premises, PREMISE_CAPACITY,
		substitution_certificate_premises, PREMISE_CAPACITY
	);
	prototype_parametricity_work_db_init(&work_db, work_items, GOAL_CAPACITY);
	prototype_parametricity_residual_db_init(&residual_db, residuals, 32);
	prototype_hott_action_db_init(
		&action_db,
		action_requests,
		256,
		action_certificates,
		256,
		action_results,
		256
	);

	uint32_t empty = prototype_context_empty(&context_db);
	uint32_t universe;
	uint32_t text;
	uint32_t int_type;
	uint32_t left_text;
	uint32_t right_text;
	if (prototype_term_universe_var(&term_db, 0, &universe) != 0 ||
		prototype_term_primitive_text(&term_db, &text) != 0 ||
		prototype_term_primitive_int(&term_db, &int_type) != 0 ||
		prototype_term_text_literal(&term_db, 10, &left_text) != 0 ||
		prototype_term_text_literal(&term_db, 11, &right_text) != 0) {
		return 1;
	}

	uint32_t left_operation = add_operation(
		&operation_graph, empty, left_text, text,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t right_operation = add_operation(
		&operation_graph, empty, right_text, text,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t sibling_operation = add_operation(
		&operation_graph, empty, left_text, text,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t text_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, text, empty,
		PROTOTYPE_INVALID_ID, text, universe
	);
	uint32_t left_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, left_operation, empty,
		left_operation, left_text, text
	);
	uint32_t right_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, right_operation, empty,
		right_operation, right_text, text
	);
	uint32_t sibling_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, sibling_operation, empty,
		sibling_operation, left_text, text
	);
	judgement.derivations[0] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO, left_has_type
	);
	judgement.derivations[1] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION, left_has_type
	);
	judgement.derivations[1].premise_count = 1;
	judgement.derivations[1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(sibling_has_type)
		};
	judgement.derivation_count = 2;
	uint32_t derivation_ids[2];
	size_t derivation_count;
	if (prototype_judgement_claim_derivations(
			&judgement, left_has_type, derivation_ids, 2, &derivation_count
		) != 0 || derivation_count != 2 || derivation_ids[0] == derivation_ids[1]) {
		return 2;
	}

	uint32_t bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &kernel_builder,
			empty, &bridge
		) != 0 || prototype_hott_bridge_db_validate(
			&bridge_db, &kernel_view
		) != 0 || bridge >= bridge_db.certificate_count ||
		bridge_db.certificates[bridge].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY) {
		return 3;
	}

	uint32_t text_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, right_has_type, bridge,
			&text_goal
		) != 0 || text_goal != 0 ||
		prototype_parametricity_relation_goal_db_validate(
			&goal_db, &kernel_view, &bridge_db
		) != 0) {
		return 4;
	}
	uint32_t interned;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, right_has_type, bridge,
			&interned
		) != 0 || interned != text_goal || goal_db.goal_count != 1) {
		return 5;
	}
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, sibling_has_type, right_has_type, bridge,
			&interned
		) != 0 || interned == text_goal) {
		return 6;
	}

	uint32_t work_id;
	if (prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, text_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ||
		strcmp(
			work_items[work_id].outcome.calculus_fingerprint,
			PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
		) != 0) {
		return 7;
	}
	uint32_t residual_id;
	if (prototype_parametricity_residual_db_add_from_work(
			&residual_db, &work_db, work_id, &residual_id
		) != 0 || prototype_parametricity_residual_db_validate(
			&residual_db, &work_db
		) != 0 || prototype_parametricity_residual_db_require_artifact_empty(
			&residual_db
		) == 0) {
		return 8;
	}

	uint32_t diagonal_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, left_has_type, left_has_type, bridge,
			&diagonal_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, diagonal_goal, PROTOTYPE_HOTT_RULE_REL_DIAGONAL
		) != 1) {
		return 9;
	}
	uint32_t repeated_diagonal_work;
	size_t work_count_before_repeat = work_db.item_count;
	size_t candidate_count_before_repeat = candidate_db.candidate_count;
	if (prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, diagonal_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32,
			&repeated_diagonal_work
		) != 0 || repeated_diagonal_work != work_id ||
		work_db.item_count != work_count_before_repeat ||
		candidate_db.candidate_count != candidate_count_before_repeat) {
		return 146;
	}
	uint32_t identity_binder = prototype_term_new_binding(&term_db);
	uint32_t identity_variable;
	uint32_t identity_lambda;
	uint32_t beta_text;
	if (identity_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, identity_binder, &identity_variable
		) != 0 || prototype_term_lambda(
			&term_db, identity_binder, identity_variable, &identity_lambda
		) != 0 || prototype_term_app(
			&term_db, identity_lambda, left_text, &beta_text
		) != 0) {
		return 39;
	}
	uint32_t beta_operation = add_operation(
		&operation_graph, empty, beta_text, text,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t beta_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, beta_operation, empty,
		beta_operation, beta_text, text
	);
	uint32_t conversion_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			text_is_type, text_is_type, beta_claim, left_has_type, bridge,
			&conversion_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL,
			conversion_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
		count_rule(
			&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_REL_CONVERT
		) != 1) {
		return 40;
	}

	uint32_t binder = prototype_term_new_binding(&term_db);
	uint32_t extended;
	if (binder == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&context_db, empty, binder, text, PROTOTYPE_INVALID_ID, &extended
		) != 0) {
		return 10;
	}
	uint32_t context_certificate;
	if (prototype_cwf_certificate_db_add_context(
		&cwf_certificate_db, &context_db, &term_db, &type_db, &judgement,
			extended, text_is_type, &context_certificate
		) != 0 || prototype_cwf_certificate_db_validate_contexts(
		&cwf_certificate_db, &context_db, &term_db, &type_db, &judgement
		) != 0) {
		return 11;
	}
	uint32_t nonempty_bridge;
	if (prototype_hott_bridge_db_construct(
			&bridge_db, &kernel_builder,
			extended, &nonempty_bridge
		) != 1) {
		return 12;
	}
	struct prototype_cwf_certificate forged_context_certificates[2] = {
		cwf_certificates[0], cwf_certificates[0]
	};
	struct prototype_cwf_certificate_db forged_context_db = {
		.certificates = forged_context_certificates,
		.certificate_count = 2,
		.certificate_capacity = 2
	};
	if (prototype_cwf_certificate_db_validate_contexts(
			&forged_context_db, &context_db, &term_db, &type_db, &judgement
		) == 0) {
		return 13;
	}
	struct prototype_cwf_certificate forged_cross_kind =
		cwf_certificates[context_certificate];
	forged_cross_kind.kind = PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION;
	struct prototype_cwf_certificate_db forged_cross_kind_db = {
		.certificates = &forged_cross_kind,
		.certificate_count = 1,
		.certificate_capacity = 1
	};
	if (prototype_cwf_certificate_db_get_kind(
			&cwf_certificate_db,
			context_certificate,
			PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
		) != NULL || prototype_cwf_certificate_db_validate(
			&forged_cross_kind_db,
			&context_db,
			&substitution_db,
			&term_db,
			&type_db,
			&judgement
		) == 0) {
		return 136;
	}
	struct prototype_cwf_certificate forged_wrong_claim =
		cwf_certificates[context_certificate];
	forged_wrong_claim.claim_id = left_has_type;
	struct prototype_cwf_certificate_db forged_wrong_claim_db = {
		.certificates = &forged_wrong_claim,
		.certificate_count = 1,
		.certificate_capacity = 1
	};
	if (prototype_cwf_certificate_db_validate_contexts(
			&forged_wrong_claim_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement
		) == 0) {
		return 137;
	}
	struct prototype_judgement_proposition foreign_propositions[1];
	struct prototype_judgement_derivation_candidate
		foreign_derivation_candidates[1];
	struct prototype_judgement_claim foreign_claims[1];
	struct prototype_judgement_derivation foreign_derivations[1];
	struct prototype_judgement_candidate_premise foreign_candidate_premises[1];
	struct prototype_judgement_premise_edge foreign_accepted_premises[1];
	struct prototype_judgement_db foreign_judgement;
	prototype_judgement_db_init(
		&foreign_judgement,
		foreign_propositions,
		foreign_derivation_candidates,
		foreign_claims,
		foreign_derivations,
		1,
		foreign_candidate_premises,
		1,
		foreign_accepted_premises,
		1
	);
	struct prototype_kernel_view mixed_kernel_view = kernel_view;
	mixed_kernel_view.judgement = &foreign_judgement;
	if (prototype_kernel_view_validate(&mixed_kernel_view) == 0) {
		return 139;
	}

	uint32_t bool_view = add_bool_view(
		&term_db, &type_db, empty, universe, text
	);
	uint32_t box_view;
	uint32_t dependent_box_view;
	uint32_t nat_view;
	if (getenv("A_PROGRAM_HOTT_REVERSE_INDEPENDENT_TYPES")) {
		nat_view = add_nat_view(
			&term_db, &type_db, &context_db, empty, universe, text
		);
		dependent_box_view = add_dependent_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		box_view = add_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
	} else {
		box_view = add_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		dependent_box_view = add_dependent_box_view(
			&term_db,
			&type_db,
			&context_db,
			empty,
			universe,
			bool_view,
			text
		);
		nat_view = add_nat_view(
			&term_db, &type_db, &context_db, empty, universe, text
		);
	}
	if (box_view == PROTOTYPE_INVALID_ID ||
		dependent_box_view == PROTOTYPE_INVALID_ID ||
		nat_view == PROTOTYPE_INVALID_ID) {
		return 73;
	}
	uint32_t bool_false;
	uint32_t bool_true;
	if (prototype_term_constructor(&term_db, bool_view, 0, &bool_false) != 0 ||
		prototype_term_constructor(&term_db, bool_view, 1, &bool_true) != 0) {
		return 14;
	}
	uint32_t false_operation = add_operation(
		&operation_graph, empty, bool_false, bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t true_operation = add_operation(
		&operation_graph, empty, bool_true, bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t bool_has_type;
	uint32_t bool_is_type;
	if (prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_view,
			universe,
			&bool_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			bool_view,
			universe,
			bool_has_type,
			&bool_is_type
		) != 0) {
		return 221;
	}
	struct prototype_hott_type_former_descriptor bool_descriptor;
	if (prototype_hott_type_former_descriptor_query(
			&term_db,
			&type_db,
			&context_db,
			&judgement,
			bool_is_type,
			&bool_descriptor
		) != 0 || bool_descriptor.kind !=
			PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT ||
		bool_descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED ||
		bool_descriptor.capabilities.artifact !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED) {
		return 222;
	}
	uint32_t box_has_type;
	uint32_t box_is_type;
	uint32_t dependent_box_has_type;
	uint32_t dependent_box_is_type;
	uint32_t nat_has_type;
	uint32_t nat_is_type;
	if (prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			box_view,
			universe,
			&box_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			box_view,
			universe,
			box_has_type,
			&box_is_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			dependent_box_view,
			universe,
			&dependent_box_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			dependent_box_view,
			universe,
			dependent_box_has_type,
			&dependent_box_is_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			nat_view,
			universe,
			&nat_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			nat_view,
			universe,
			nat_has_type,
			&nat_is_type
		) != 0) {
		return 241;
	}
	uint32_t dependent_box_type_id = term_db.terms[
		dependent_box_view
	].as.type_view.view_type_id;
	uint32_t box_type_id = term_db.terms[box_view].as.type_view.view_type_id;
	const struct prototype_type_declaration* box_declaration =
		box_type_id < type_db.type_count ?
		&type_db.type_declarations[box_type_id] : NULL;
	const struct prototype_type_constructor_declaration* box_constructor_declaration =
		box_declaration && box_declaration->first_constructor <
			type_db.constructor_count ? &type_db.constructor_declarations[
				box_declaration->first_constructor
			] : NULL;
	const struct prototype_type_declaration* dependent_box_declaration =
		dependent_box_type_id < type_db.type_count ?
		&type_db.type_declarations[dependent_box_type_id] : NULL;
	const struct prototype_type_constructor_declaration*
		dependent_box_constructor = dependent_box_declaration &&
		dependent_box_declaration->first_constructor < type_db.constructor_count ?
		&type_db.constructor_declarations[
			dependent_box_declaration->first_constructor
		] : NULL;
	const struct prototype_context* dependent_second_context =
		dependent_box_constructor ? prototype_context_get(
			&context_db, dependent_box_constructor->field_context
		) : NULL;
	const struct prototype_context* dependent_first_context =
		dependent_second_context ? prototype_context_get(
			&context_db, dependent_second_context->parent
		) : NULL;
	uint32_t dependent_first_context_certificate;
	uint32_t box_field_context_certificate;
	uint32_t dependent_second_classifier_claim;
	uint32_t dependent_second_is_type;
	uint32_t dependent_second_context_certificate;
	uint32_t dependent_classifier = dependent_second_context ?
		prototype_context_classifier_term(dependent_second_context) :
		PROTOTYPE_INVALID_ID;
	const struct prototype_term* dependent_application =
		dependent_classifier < term_db.term_count ?
		&term_db.terms[dependent_classifier] : NULL;
	uint32_t dependent_family = dependent_application &&
		dependent_application->tag == PROTOTYPE_TERM_APP ?
		dependent_application->as.app.function : PROTOTYPE_INVALID_ID;
	const struct prototype_term* dependent_family_lambda =
		dependent_family < term_db.term_count ?
		&term_db.terms[dependent_family] : NULL;
	uint32_t dependent_family_context;
	uint32_t dependent_family_context_certificate;
	uint32_t dependent_family_projection;
	uint32_t bool_has_type_in_family_context;
	uint32_t dependent_family_binder_claim;
	uint32_t dependent_family_classifier;
	uint32_t dependent_family_claim;
	uint32_t dependent_first_projection;
	uint32_t dependent_family_claim_in_first_context;
	uint32_t dependent_first_variable_claim;
	if (!box_constructor_declaration || !dependent_first_context ||
		!dependent_second_context ||
		!dependent_application || !dependent_family_lambda ||
		dependent_family_lambda->tag != PROTOTYPE_TERM_LAMBDA ||
		prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			box_constructor_declaration->field_context,
			bool_is_type,
			&box_field_context_certificate
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_second_context->parent,
			bool_is_type,
			&dependent_first_context_certificate
		) != 0 || prototype_context_extend(
			&context_db,
			empty,
			dependent_family_lambda->as.lambda.binding_id,
			bool_view,
			PROTOTYPE_INVALID_ID,
			&dependent_family_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_family_context,
			bool_is_type,
			&dependent_family_context_certificate
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			dependent_family_context,
			empty,
			&dependent_family_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			bool_has_type,
			dependent_family_projection,
			&bool_has_type_in_family_context
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			dependent_family_context,
			dependent_family_lambda->as.lambda.binding_id,
			bool_view,
			&dependent_family_binder_claim
		) != 0 || prototype_term_pi(
			&term_db,
			bool_view,
			universe,
			&dependent_family_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			empty,
			dependent_family,
			dependent_family_classifier,
			dependent_family_binder_claim,
			bool_has_type_in_family_context,
			&dependent_family_claim
		) != 0 || prototype_substitution_projection_path(
			&substitution_db,
			&context_db,
			dependent_second_context->parent,
			empty,
			&dependent_first_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			dependent_family_claim,
			dependent_first_projection,
			&dependent_family_claim_in_first_context
		) != 0 || prototype_judgement_add_context_binding_assumption(
			&judgement,
			&term_db,
			&context_db,
			dependent_second_context->parent,
			dependent_first_context->binding_id,
			bool_view,
			&dependent_first_variable_claim
		) != 0 || prototype_judgement_add_app_claim(
			&judgement,
			&term_db,
			&type_db,
			&context_db,
			&substitution_db,
			dependent_second_context->parent,
			dependent_classifier,
			universe,
			dependent_family_claim_in_first_context,
			dependent_first_variable_claim,
			&dependent_second_classifier_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			dependent_second_context->parent,
			prototype_context_classifier_term(dependent_second_context),
			universe,
			dependent_second_classifier_claim,
			&dependent_second_is_type
		) != 0 || prototype_cwf_certificate_db_add_context(
			&cwf_certificate_db,
			&context_db,
			&term_db,
			&type_db,
			&judgement,
			dependent_box_constructor->field_context,
			dependent_second_is_type,
			&dependent_second_context_certificate
		) != 0) {
		return 308;
	}
	(void)box_field_context_certificate;
	(void)dependent_family_context_certificate;
	uint32_t false_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, false_operation, empty,
		false_operation, bool_false, bool_view
	);
	uint32_t true_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, true_operation, empty,
		true_operation, bool_true, bool_view
	);
	uint32_t bool_false_constructor_claim;
	uint32_t bool_true_constructor_claim;
	if (prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_false,
			bool_view,
			&bool_false_constructor_claim
		) != 0 || prototype_judgement_add_constructor_intro_claim(
			&judgement,
			&term_db,
			&type_db,
			empty,
			bool_true,
			bool_view,
			&bool_true_constructor_claim
		) != 0) {
		return 240;
	}
	false_claim = bool_false_constructor_claim;
	true_claim = bool_true_constructor_claim;
	uint32_t bool_distinct_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, false_claim, true_claim, bridge,
			&bool_distinct_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_distinct_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_READY ||
			count_rule(
				&candidate_db, bool_distinct_goal,
				PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT
			) != 1) {
		return 15;
	}
	uint32_t bool_distinct_candidate = find_rule(
		&candidate_db,
		bool_distinct_goal,
		PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT
	);
	if (bool_distinct_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[bool_distinct_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY) {
		return 147;
	}
	uint32_t heterogeneous_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db,
			PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, text_is_type,
			false_claim, left_has_type, bridge, &heterogeneous_goal
		) != 0 || prototype_parametricity_relation_goal_db_validate(
			&goal_db, &kernel_view, &bridge_db
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db,
			NULL, heterogeneous_goal, PROTOTYPE_INVALID_ID,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF, 32, &work_id
		) != 0 || work_items[work_id].outcome.state !=
			PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE) {
		return 144;
	}
	uint32_t bool_diagonal_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, false_claim, false_claim, bridge,
			&bool_diagonal_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, bool_diagonal_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, bool_diagonal_goal, PROTOTYPE_HOTT_RULE_REL_DIAGONAL
		) != 1 || count_rule(
			&candidate_db, bool_diagonal_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 1) {
		return 16;
	}
	uint32_t bool_constructor_candidate = find_rule(
		&candidate_db,
		bool_diagonal_goal,
		PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
	);
	if (bool_constructor_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[bool_constructor_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS) {
		return 147;
	}

	uint32_t neutral;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){
				.namespace_symbol_id = 7,
				.name_symbol_id = 8
			},
			&neutral
		) != 0) {
		return 17;
	}
	uint32_t neutral_operation = add_operation(
		&operation_graph, empty, neutral, bool_view,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t neutral_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, neutral_operation, empty,
		neutral_operation, neutral, bool_view
	);
	uint32_t neutral_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			bool_is_type, bool_is_type, neutral_claim, false_claim, bridge,
			&neutral_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, neutral_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, neutral_goal, PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION
		) != 1 || count_rule(
			&candidate_db, neutral_goal,
			PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR
		) != 0) {
		return 18;
	}
	uint32_t neutral_candidate = find_rule(
		&candidate_db,
		neutral_goal,
		PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION
	);
	if (neutral_candidate == PROTOTYPE_INVALID_ID ||
		candidate_db.candidates[neutral_candidate].object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_DEFERRED) {
		return 147;
	}
	uint32_t universe_successor;
	if (prototype_term_universe_var(&term_db, 1, &universe_successor) != 0) {
		return 35;
	}
	uint32_t bool_type_operation = add_operation(
		&operation_graph, empty, bool_view, universe,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t text_type_operation = add_operation(
		&operation_graph, empty, text, universe,
		PROTOTYPE_TERM_CATEGORY_VALUE
	);
	uint32_t universe_has_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE, universe, empty,
		PROTOTYPE_INVALID_ID, universe, universe_successor
	);
	if (universe_has_type == PROTOTYPE_INVALID_ID ||
		judgement.derivation_count >= judgement.derivation_capacity) {
		return 35;
	}
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY, universe_has_type
	);
	uint32_t universe_is_type;
	if (prototype_judgement_add_is_type_claim(
			&judgement,
			&term_db,
			empty,
			universe,
			universe_successor,
			universe_has_type,
			&universe_is_type
		) != 0) {
		return 35;
	}
	uint32_t bool_has_universe = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, bool_type_operation, empty,
		bool_type_operation, bool_view, universe
	);
	uint32_t text_has_universe = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION, text_type_operation, empty,
		text_type_operation, text, universe
	);
	uint32_t universe_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			universe_is_type, universe_is_type,
			bool_has_universe, text_has_universe, bridge,
			&universe_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, universe_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_UNIVERSE) {
		return 36;
	}

	uint32_t empty_row;
	uint32_t print_row;
	uint32_t pure_comp;
	uint32_t effect_comp;
	uint32_t returned_left;
	uint32_t returned_right;
	if (prototype_term_effect_row_empty(&term_db, &empty_row
		) != 0 || prototype_term_effect_row_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, empty_row, &print_row
		) != 0 || prototype_term_computation_type(
			&term_db, empty_row, text, &pure_comp
		) != 0 || prototype_term_computation_type(
			&term_db, print_row, text, &effect_comp
		) != 0 || prototype_term_return(
			&term_db, left_text, &returned_left
		) != 0 || prototype_term_return(
			&term_db, right_text, &returned_right
		) != 0) {
		return 19;
	}
	uint32_t pure_comp_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_comp, empty,
		PROTOTYPE_INVALID_ID, pure_comp, universe
	);
	uint32_t returned_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, pure_comp
	);
	uint32_t returned_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, pure_comp
	);
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO, returned_left_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(left_has_type)
		};
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO, returned_right_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(right_has_type)
		};
	uint32_t computation_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			pure_comp_is_type, pure_comp_is_type,
			returned_left_claim, returned_right_claim, bridge,
			&computation_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, computation_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, computation_goal, PROTOTYPE_HOTT_RULE_REL_COMP_RETURN
		) != 1) {
		return 20;
	}
	uint32_t thunk_type;
	uint32_t thunk_left;
	uint32_t thunk_right;
	if (prototype_term_thunk_type(
			&term_db, pure_comp, &thunk_type
		) != 0 || prototype_term_thunk(
			&term_db, returned_left, &thunk_left
		) != 0 || prototype_term_thunk(
			&term_db, returned_right, &thunk_right
		) != 0) {
		return 33;
	}
	uint32_t thunk_type_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, thunk_type, empty,
		PROTOTYPE_INVALID_ID, thunk_type, universe
	);
	uint32_t thunk_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_left, empty,
		PROTOTYPE_INVALID_ID, thunk_left, thunk_type
	);
	uint32_t thunk_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, thunk_right, empty,
		PROTOTYPE_INVALID_ID, thunk_right, thunk_type
	);
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO, thunk_left_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(returned_left_claim)
		};
	judgement.derivations[judgement.derivation_count++] = fixture_derivation(
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO, thunk_right_claim
	);
	judgement.derivations[judgement.derivation_count - 1].premise_count = 1;
	judgement.derivations[judgement.derivation_count - 1].premises =
		(struct prototype_judgement_premise_edge[]) {
			fixture_premise(returned_right_claim)
		};
	uint32_t thunk_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_VALUE,
			thunk_type_is_type, thunk_type_is_type,
			thunk_left_claim, thunk_right_claim, bridge,
			&thunk_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, thunk_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, thunk_goal, PROTOTYPE_HOTT_RULE_REL_THUNK_PURE
		) != 1) {
		return 34;
	}

	uint32_t effect_comp_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_comp, empty,
		PROTOTYPE_INVALID_ID, effect_comp, universe
	);
	uint32_t effect_left_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_left, empty,
		PROTOTYPE_INVALID_ID, returned_left, effect_comp
	);
	uint32_t effect_right_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, returned_right, empty,
		PROTOTYPE_INVALID_ID, returned_right, effect_comp
	);
	uint32_t effect_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_comp_is_type, effect_comp_is_type,
			effect_left_claim, effect_right_claim, bridge,
			&effect_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, effect_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 21;
	}
	uint32_t continuation_binder = prototype_term_new_binding(&term_db);
	uint32_t continuation_variable;
	uint32_t continuation_return;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t print_operation;
	uint32_t request;
	if (continuation_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			&term_db, continuation_binder, &continuation_variable
		) != 0 || prototype_term_return(
			&term_db, continuation_variable, &continuation_return
		) != 0 || prototype_term_lambda(
			&term_db, continuation_binder, continuation_return,
			&continuation_lambda
		) != 0 || prototype_term_thunk(
			&term_db, continuation_lambda, &continuation_thunk
		) != 0 || prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &print_operation
		) != 0 || prototype_term_operation_request(
			&term_db, print_operation, left_text, continuation_thunk, &request
		) != 0) {
		return 37;
	}
	uint32_t request_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, request, empty,
		PROTOTYPE_INVALID_ID, request, effect_comp
	);
	uint32_t request_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_comp_is_type, effect_comp_is_type,
			request_claim, request_claim, bridge,
			&request_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, request_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED ||
		work_items[work_id].outcome.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST) {
		return 38;
	}

	uint32_t family_binder = prototype_term_new_binding(&term_db);
	uint32_t pure_family;
	uint32_t effect_family;
	uint32_t pure_pi;
	uint32_t effect_pi;
	if (family_binder == PROTOTYPE_INVALID_ID || prototype_term_pure_family(
			&term_db, family_binder, pure_comp, &pure_family
		) != 0 || prototype_term_pure_family(
			&term_db, family_binder, effect_comp, &effect_family
		) != 0 || prototype_term_pi_family(
			&term_db, text, pure_family, &pure_pi
		) != 0 || prototype_term_pi_family(
			&term_db, text, effect_family, &effect_pi
		) != 0) {
		return 22;
	}
	uint32_t pure_function;
	uint32_t effect_function;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ .namespace_symbol_id = 9,
				.name_symbol_id = 1 },
			&pure_function
		) != 0 || prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ .namespace_symbol_id = 9,
				.name_symbol_id = 2 },
			&effect_function
		) != 0) {
		return 23;
	}
	uint32_t pure_pi_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, pure_pi, empty,
		PROTOTYPE_INVALID_ID, pure_pi, universe
	);
	uint32_t effect_pi_is_type = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION, effect_pi, empty,
		PROTOTYPE_INVALID_ID, effect_pi, universe
	);
	uint32_t pure_function_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, pure_function, empty,
		PROTOTYPE_INVALID_ID, pure_function, pure_pi
	);
	uint32_t effect_function_claim = fixture_add_claim(
		&judgement, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER, effect_function, empty,
		PROTOTYPE_INVALID_ID, effect_function, effect_pi
	);
	uint32_t pure_pi_goal;
	uint32_t effect_pi_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			pure_pi_is_type, pure_pi_is_type,
			pure_function_claim, pure_function_claim, bridge,
			&pure_pi_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, pure_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || count_rule(
			&candidate_db, pure_pi_goal, PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE
		) != 1 || prototype_parametricity_relation_goal_db_intern(
			&goal_db, &kernel_view, &bridge_db, PROTOTYPE_HOTT_RELATION_COMPUTATION,
			effect_pi_is_type, effect_pi_is_type,
			effect_function_claim, effect_function_claim, bridge,
			&effect_pi_goal
		) != 0 || prototype_parametricity_relation_plan(
			&goal_db, &candidate_db, &work_db, &kernel_view, &bridge_db, NULL, effect_pi_goal,
			PROTOTYPE_INVALID_ID, PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			32, &work_id
		) != 0 || work_items[work_id].outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL ||
		work_items[work_id].outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL) {
		return 24;
	}

	if (prototype_parametricity_candidate_db_validate(
			&candidate_db, &goal_db, &kernel_view
		) != 0 || prototype_parametricity_work_db_validate(
			&work_db, &goal_db, &candidate_db
		) != 0) {
		return 25;
	}
	uint32_t conversion_candidate = find_rule(
		&candidate_db, conversion_goal, PROTOTYPE_HOTT_RULE_REL_CONVERT
	);
	if (conversion_candidate == PROTOTYPE_INVALID_ID) {
		return 41;
	}
	uint32_t conversion_edge =
		candidate_db.candidates[conversion_candidate].first_conversion_premise;
	uint64_t saved_revision =
		candidate_db.conversion_premises[conversion_edge].conversion_graph_revision;
	++candidate_db.conversion_premises[conversion_edge].conversion_graph_revision;
	if (prototype_parametricity_candidate_db_validate(
			&candidate_db, &goal_db, &kernel_view
		) == 0) {
		return 42;
	}
	candidate_db.conversion_premises[conversion_edge].conversion_graph_revision =
		saved_revision;

	/* Two derivations may share one child goal; adjacency belongs to candidates. */
	struct prototype_parametricity_candidate shared_candidates[2] = {
		{
			.id = 0,
			.conclusion_goal_id = bool_distinct_goal,
			.rule = PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR,
			.object_result = PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS,
			.first_child_edge = 0,
			.child_edge_count = 1
		},
		{
			.id = 1,
			.conclusion_goal_id = bool_diagonal_goal,
			.rule = PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR,
			.object_result = PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS,
			.first_child_edge = 1,
			.child_edge_count = 1
		}
	};
	struct prototype_parametricity_child_edge shared_edges[2] = {
		{ .candidate_id = 0, .child_goal_id = diagonal_goal,
			.role = PROTOTYPE_HOTT_CHILD_ADT_FIELD, .ordinal = 0 },
		{ .candidate_id = 1, .child_goal_id = diagonal_goal,
			.role = PROTOTYPE_HOTT_CHILD_ADT_FIELD, .ordinal = 0 }
	};
	struct prototype_parametricity_candidate_db shared_db;
	prototype_parametricity_candidate_db_init(
		&shared_db, shared_candidates, 2, NULL, 0, shared_edges, 2,
		NULL, 0, NULL, 0, NULL, 0
	);
	shared_db.candidate_count = 2;
	shared_db.child_edge_count = 2;
	if (prototype_parametricity_candidate_db_validate(
			&shared_db, &goal_db, &kernel_view
		) != 0) {
		return 26;
	}
	shared_edges[1].child_goal_id = bool_diagonal_goal;
	if (prototype_parametricity_candidate_db_validate(
			&shared_db, &goal_db, &kernel_view
		) == 0) {
		return 27;
	}

	uint32_t action_id;
	struct prototype_hott_action_request context_action = {
		.kind = PROTOTYPE_HOTT_ACTION_CONTEXT,
		.key.context = { .source_context_id = empty }
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, context_action, &action_id
		) != 0 || action_id != 0) {
		return 28;
	}
	uint32_t context_certificate_id;
	struct prototype_hott_action_certificate context_action_certificate = {
		.request_id = action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE,
		.data.context = { .result_bridge_id = bridge }
	};
	if (prototype_hott_action_certificate_add(
			&action_db, &kernel_view, &bridge_db, context_action_certificate,
			&context_certificate_id
		) != 0) {
		return 29;
	}
	struct prototype_hott_action_result context_result = {
		.request_id = action_id,
		.certificate_id = context_certificate_id,
		.outcome = {
			.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			.step_limit = 0,
			.term_graph_revision = term_db.term_count
		}
	};
	memcpy(
		context_result.outcome.calculus_fingerprint,
		PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
		65
	);
	uint32_t result_id;
	if (prototype_hott_action_result_publish(
			&action_db, context_result, &result_id
		) != 0 || result_id != 0) {
		return 30;
	}
	struct prototype_hott_action_request type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = text_is_type,
			.source_bridge_id = bridge
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, type_action, &action_id
		) != 0) {
		return 31;
	}
	uint32_t type_action_id = action_id;
	uint32_t repeated_type_action_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, type_action, &repeated_type_action_id
		) != 0 || repeated_type_action_id != type_action_id ||
		action_db.request_count != 2) {
		return 32;
	}
	struct prototype_hott_action_certificate wrong_kind_certificate = {
		.request_id = type_action_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE,
		.data.context = { .result_bridge_id = bridge }
	};
	if (prototype_hott_action_certificate_add(
			&action_db, &kernel_view, &bridge_db, wrong_kind_certificate,
			&(uint32_t){0}
		) == 0) {
		return 33;
	}
	struct prototype_hott_action_request term_action = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = left_has_type,
			.source_bridge_id = bridge,
			.relation_type_action_request_id = type_action_id
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, term_action, &action_id
		) != 0) {
		return 34;
	}
	uint32_t identity;
	if (prototype_substitution_identity(
			&substitution_db, &context_db, empty, &identity
		) != 0) {
		return 35;
	}
	struct prototype_hott_action_request substitution_action = {
		.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
		.key.substitution = {
			.source_substitution_id = identity,
			.source_bridge_id = bridge,
			.target_bridge_id = bridge
		}
	};
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, substitution_action, &action_id
		) != 0 || prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 36;
	}
	uint32_t identity_substitution_action_id = action_id;

	struct prototype_hott_action_request bool_type_action = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_type_action_id;
	uint32_t bool_relation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db, &kernel_view, &bridge_db, bool_type_action, &bool_type_action_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			&action_db, &kernel_builder, &bridge_db, bool_type_action_id, &result_id
		) != 0 || action_results[result_id].outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 41;
	}
	bool_relation_result_id = result_id;
	struct prototype_hott_action_request bool_identity_computation = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = bool_is_type,
			.source_bridge_id = bridge
		}
	};
	uint32_t bool_identity_computation_id;
	uint32_t repeated_bool_identity_computation_id;
	uint32_t bool_identity_computation_result_id;
	if (prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation,
			&bool_identity_computation_id
		) != 0 || prototype_hott_action_request_intern(
			&action_db,
			&kernel_view,
			&bridge_db,
			bool_identity_computation,
			&repeated_bool_identity_computation_id
		) != 0 || bool_identity_computation_id !=
			repeated_bool_identity_computation_id ||
		bool_identity_computation_id == bool_type_action_id ||
		prototype_hott_execute_identity_type_computation(
			&action_db,
			&kernel_builder,
			&bridge_db,
			bool_identity_computation_id,
			&bool_identity_computation_result_id
		) != 0 || action_results[
			bool_identity_computation_result_id
		].outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		action_results[bool_identity_computation_result_id].certificate_id >=
			action_db.certificate_count ||
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		].data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		type_db.type_declarations[
			action_db.certificates[
				action_results[bool_identity_computation_result_id].certificate_id
			].data.identity_type.generated_type_declaration_id
		].index_count != 2 ||
		prototype_hott_action_db_validate(
			&action_db, &kernel_view, &bridge_db
		) != 0) {
		return 137;
	}
	const struct prototype_hott_action_certificate* bool_identity_certificate =
		&action_db.certificates[
			action_results[bool_identity_computation_result_id].certificate_id
		];
