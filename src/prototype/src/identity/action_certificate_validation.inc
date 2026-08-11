static int hott_action_request_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_request* request,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !request || request->id != expected_id ||
		!hott_action_kind_is_valid(request->kind) ||
		request->key_hash != hott_action_request_hash(request)) {
		return 0;
	}
	switch (request->kind) {
	case PROTOTYPE_HOTT_ACTION_CONTEXT:
		return request->key.context.source_context_id < contexts->context_count &&
			hott_context_is_formed(
				contexts,
				cwf_certificates,
				request->key.context.source_context_id
			);
	case PROTOTYPE_HOTT_ACTION_SUBSTITUTION:
		{
				const struct prototype_substitution* substitution =
					prototype_substitution_get(
						substitutions,
						request->key.substitution.source.substitution_id
					);
				struct prototype_certified_substitution_ref certified_source;
			const struct prototype_hott_bridge* source_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.source_bridge_id
				);
			const struct prototype_hott_bridge* target_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.target_bridge_id
				);
				return substitution && prototype_cwf_certified_substitution_ref_get(
						cwf_certificates,
						substitutions,
						judgement,
						request->key.substitution.source.substitution_id,
						request->key.substitution.source.certificate_id,
						&certified_source
					) == 0 && source_bridge && target_bridge &&
				source_bridge->source_context_id ==
					substitution->source_context &&
				target_bridge->source_context_id ==
					substitution->target_context;
		}
	case PROTOTYPE_HOTT_ACTION_RELATION_TYPE:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, request->key.relation_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.relation_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			return claim && bridge && bridge_certificate &&
				(bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY ||
				 bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) &&
				bridge->source_context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				hott_is_type_claim_matches(
					terms,
					type_declarations,
					judgement,
					request->key.relation_type.source_claim_id,
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id,
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject
				);
		}
	case PROTOTYPE_HOTT_ACTION_TERM:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, request->key.term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.term.source_bridge_id
				);
			const struct prototype_hott_action_request* type_request =
				request->key.term.relation_type_action_request_id < expected_id ?
				&db->requests[
					request->key.term.relation_type_action_request_id
				] : NULL;
			const struct prototype_judgement_claim* type_claim =
				type_request && type_request->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
				prototype_judgement_claim_get(
					judgement, type_request->key.relation_type.source_claim_id
				) : NULL;
			return claim &&
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, judgement, claim) &&
				bridge &&
				bridge->source_context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				type_request &&
				type_request->key.relation_type.source_bridge_id ==
					request->key.term.source_bridge_id &&
				type_claim &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->subject == prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.identity_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.identity_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			return source && bridge && bridge_certificate &&
				(bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY ||
				 bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) &&
				bridge->source_context_id ==
					prototype_judgement_claim_proposition(
						judgement,
						request->key.identity_type.source_claim_id
					)->context_id &&
				hott_is_type_claim_matches(
					terms,
					type_declarations,
					judgement,
					request->key.identity_type.source_claim_id,
					bridge->source_context_id,
					prototype_judgement_claim_proposition(
						judgement,
						request->key.identity_type.source_claim_id
					)->subject
				);
		}
	case PROTOTYPE_HOTT_ACTION_OBJECT_TERM:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.object_term.source_claim_id
				);
			const struct prototype_judgement_proposition* source_proposition =
				source ? prototype_judgement_proposition_get(
					judgement, source->proposition_id
				) : NULL;
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.object_term.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			const struct prototype_hott_action_request* identity_request =
				request->key.object_term.identity_type_action_request_id < expected_id ?
				&db->requests[
					request->key.object_term.identity_type_action_request_id
				] : NULL;
			const struct prototype_judgement_proposition* identity_source =
				identity_request && identity_request->kind ==
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
				prototype_judgement_claim_proposition(
					judgement,
					identity_request->key.identity_type.source_claim_id
				) : NULL;
			return source_proposition && source_proposition->kind ==
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, judgement, source) &&
				bridge && bridge_certificate && bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
				bridge->source_context_id == source_proposition->context_id &&
				identity_request && identity_source && identity_source->kind ==
					PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				identity_request->key.identity_type.source_bridge_id == bridge->id &&
				identity_source->context_id == source_proposition->context_id &&
				identity_source->subject == source_proposition->classifier;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_request_intern(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_request request,
	uint32_t* p_request_id
) {
	if (db) {
		db->request_intern_requests++;
	}
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || !p_request_id || !db->requests ||
		db->request_count >= db->request_capacity ||
		prototype_kernel_view_validate_stores(kernel) != 0) {
		return -1;
	}
	request.id = (uint32_t)db->request_count;
	request.key_hash = hott_action_request_hash(&request);
	request.hash_next = PROTOTYPE_INVALID_ID;
	if (!hott_action_request_is_valid(
			db,
			&request,
			request.id,
			contexts,
			substitutions,
			cwf_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement
		)) {
		return -1;
	}
	size_t bucket =
		request.key_hash % PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT;
	for (uint32_t i = db->request_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = db->requests[i].hash_next) {
		if (i >= db->request_count) {
			return -1;
		}
		if (db->requests[i].key_hash == request.key_hash &&
			hott_action_request_key_equal(&db->requests[i], &request)) {
			db->request_intern_hits++;
			*p_request_id = i;
			return 0;
		}
	}
	request.hash_next = db->request_index_heads[bucket];
	db->requests[db->request_count] = request;
	db->request_index_heads[bucket] = request.id;
	db->request_count++;
	*p_request_id = request.id;
	return 0;
}

static const struct prototype_judgement_derivation*
hott_certificate_derivation(
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

static int hott_claim_has_match_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t remaining_depth
) {
	if (!judgement || remaining_depth == 0 || claim_id >= judgement->claim_count) {
		return 0;
	}
	if (hott_certificate_derivation(
			judgement, claim_id, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
		)) {
		return 1;
	}
	const struct prototype_judgement_derivation* conversion =
		hott_certificate_derivation(
			judgement, claim_id, PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
		);
	const struct prototype_judgement_derivation* reindex =
		hott_certificate_derivation(
			judgement,
			claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	const struct prototype_judgement_derivation* wrapper = conversion ?
		conversion : reindex;
	return wrapper && wrapper->premise_count == 1 &&
		hott_claim_has_match_derivation(
			judgement,
			wrapper->premises[0].claim_id,
			remaining_depth - 1
		);
}

static int hott_term_is_universe_field_projection(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t scrutinee,
	int projection
) {
	const struct prototype_term* term = term_id < terms->term_count ?
		&terms->terms[term_id] : NULL;
	if (!term || term->tag != PROTOTYPE_TERM_MATCH ||
		term->as.match.scrutinee != scrutinee || term->as.match.case_count != 1 ||
		term->as.match.first_case >= terms->case_count) {
		return 0;
	}
	const struct prototype_match_case* match_case =
		&terms->cases[term->as.match.first_case];
	return match_case->constructor_id == 0 && match_case->binder_count == 7 &&
		match_case->first_binder + (uint32_t)projection < terms->case_binder_count &&
		match_case->body < terms->term_count &&
		terms->terms[match_case->body].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[match_case->body].as.var.binding_id ==
			terms->case_binders[
				match_case->first_binder + (uint32_t)projection
			].binding_id;
}

static int hott_universe_projection_claim_is_valid(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t projection_claim_id,
	uint32_t universe_witness_claim_id,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	int projection
) {
	const struct prototype_judgement_proposition* result =
		prototype_judgement_claim_proposition(judgement, projection_claim_id);
	const struct prototype_judgement_proposition* witness =
		prototype_judgement_claim_proposition(judgement, universe_witness_claim_id);
	if (!result || !witness || result->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return 0;
	}
	if (hott_term_is_universe_field_projection(
			terms, result->subject, witness->subject, projection
		)) {
		return hott_claim_has_match_derivation(
			judgement, projection_claim_id, 4
		);
	}

	uint32_t application_claim = projection_claim_id;
	for (uint32_t depth = 0; depth < 4; ++depth) {
		const struct prototype_judgement_derivation* conversion =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			);
		const struct prototype_judgement_derivation* reindex =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
			);
		const struct prototype_judgement_derivation* wrapper = conversion ?
			conversion : reindex;
		if (!wrapper) {
			break;
		}
		if (wrapper->premise_count != 1 ||
			(reindex && (reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			 reindex->semantic_action_id == PROTOTYPE_INVALID_ID))) {
			return 0;
		}
		application_claim = wrapper->premises[0].claim_id;
	}
	uint32_t expected_arguments[3] = {
		left_endpoint, right_endpoint, witness->subject
	};
	uint32_t witness_source_claim = universe_witness_claim_id;
	for (uint32_t depth = 0; depth < 4; ++depth) {
		const struct prototype_judgement_derivation* conversion =
			hott_certificate_derivation(
				judgement,
				witness_source_claim,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			);
		const struct prototype_judgement_derivation* reindex =
			hott_certificate_derivation(
				judgement,
				witness_source_claim,
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
			);
		const struct prototype_judgement_derivation* wrapper = conversion ?
			conversion : reindex;
		if (!wrapper) {
			break;
		}
		if (wrapper->premise_count != 1 ||
			(reindex && (reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			 reindex->semantic_action_id == PROTOTYPE_INVALID_ID))) {
			return 0;
		}
		witness_source_claim = wrapper->premises[0].claim_id;
	}
	uint32_t lambda_claim = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 3; i > 0; --i) {
		const struct prototype_judgement_derivation* application =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
			);
		if (!application || application->premise_count != 2) {
			return 0;
		}
		const struct prototype_judgement_proposition* argument =
			prototype_judgement_claim_proposition(
				judgement, application->premises[1].claim_id
			);
		int argument_matches = argument &&
			(argument->subject == expected_arguments[i - 1] ||
			 prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				argument->subject,
				expected_arguments[i - 1]
			 ).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
		if (!argument_matches ||
			(i == 3 && application->premises[1].claim_id !=
				witness_source_claim)) {
			return 0;
		}
		application_claim = application->premises[0].claim_id;
		lambda_claim = application_claim;
	}

	uint32_t proof_binder_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < 3; ++i) {
		const struct prototype_judgement_proposition* lambda =
			prototype_judgement_claim_proposition(judgement, lambda_claim);
		const struct prototype_judgement_derivation* introduction =
			hott_certificate_derivation(
				judgement,
				lambda_claim,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
			);
		if (!lambda || !introduction || introduction->premise_count != 2 ||
			lambda->subject >= terms->term_count ||
			terms->terms[lambda->subject].tag != PROTOTYPE_TERM_LAMBDA) {
			return 0;
		}
		if (i == 2) {
			proof_binder_claim_id = introduction->premises[0].claim_id;
		}
		lambda_claim = introduction->premises[1].claim_id;
	}
	const struct prototype_judgement_proposition* body =
		prototype_judgement_claim_proposition(judgement, lambda_claim);
	const struct prototype_judgement_proposition* proof_binder =
		prototype_judgement_claim_proposition(
			judgement, proof_binder_claim_id
		);
	if (!body || !proof_binder || proof_binder->subject >= terms->term_count ||
		terms->terms[proof_binder->subject].tag != PROTOTYPE_TERM_VAR ||
		!hott_term_is_universe_field_projection(
			terms, body->subject, proof_binder->subject, projection
		) || !hott_claim_has_match_derivation(judgement, lambda_claim, 4)) {
		return 0;
	}
	return 1;
}

static int hott_action_certificate_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_certificate* certificate,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	(void)substitutions;
	(void)cwf_certificates;
	(void)operations;
	(void)type_declarations;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(db, certificate ?
			certificate->request_id : PROTOTYPE_INVALID_ID);
	if (!certificate || certificate->id != expected_id || !request ||
		certificate->kind != request->kind) {
		return 0;
	}
	switch (certificate->kind) {
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE:
		{
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, certificate->data.context.result_bridge_id
				);
			return request->kind == PROTOTYPE_HOTT_ACTION_CONTEXT &&
				bridge &&
				bridge->source_context_id ==
					request->key.context.source_context_id &&
				prototype_context_get(contexts, bridge->bridge_context_id);
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.relation_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.relation_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			const struct prototype_hott_relation_type_action_certificate* type =
				&certificate->data.relation_type;
			const struct prototype_context* right_context =
				prototype_context_get(contexts, type->endpoint_context_id);
			const struct prototype_context* left_context = right_context ?
				prototype_context_get(contexts, right_context->parent) : NULL;
			const struct prototype_judgement_claim* relation =
				prototype_judgement_claim_get(
					judgement, type->relation_is_type_claim_id
				);
			uint32_t left_type;
			uint32_t right_type;
			uint32_t left_endpoint;
			uint32_t right_endpoint;
			uint32_t outer_binding;
			uint32_t inner_family;
			uint32_t inner_binding;
			uint32_t relation_body;
			int relation_body_matches;
			if (request->kind != PROTOTYPE_HOTT_ACTION_RELATION_TYPE || !source ||
				!bridge || !bridge_certificate ||
				(bridge_certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
				 bridge_certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) ||
				!right_context || !left_context || !relation ||
				type->relation_family_semantics !=
					PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION ||
				type->left_context_certificate_id >=
					cwf_certificates->certificate_count ||
				type->right_context_certificate_id >=
					cwf_certificates->certificate_count ||
				cwf_certificates->certificates[
					type->left_context_certificate_id
				].structural_id != right_context->parent ||
				cwf_certificates->certificates[
					type->right_context_certificate_id
				].structural_id != type->endpoint_context_id ||
				left_context->binding_id != type->left_endpoint_binding_id ||
				right_context->binding_id != type->right_endpoint_binding_id ||
				prototype_term_pure_family_parts(
					terms,
					type->relation_family_term_id,
					&outer_binding,
					&inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					inner_family,
					&inner_binding,
					&relation_body
				) != 0 || prototype_term_core_shape_equal_under_binders(
					terms,
					(uint32_t[]) { outer_binding, inner_binding },
					(uint32_t[]) {
						type->left_endpoint_binding_id,
						type->right_endpoint_binding_id
					},
					2,
					relation_body,
					type->relation_type_term_id,
					&relation_body_matches
				) != 0 || !relation_body_matches ||
				prototype_term_relation_type_info(
					terms,
					type->relation_type_term_id,
					&left_type,
					&right_type,
					&left_endpoint,
					&right_endpoint
				) != 0) {
				return 0;
			}
			return bridge->source_context_id == prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id &&
				left_type == prototype_context_classifier_term(left_context) &&
				right_type == prototype_context_classifier_term(right_context) &&
				left_endpoint < terms->term_count &&
				terms->terms[left_endpoint].tag == PROTOTYPE_TERM_VAR &&
				terms->terms[left_endpoint].as.var.binding_id ==
					type->left_endpoint_binding_id &&
				right_endpoint < terms->term_count &&
				terms->terms[right_endpoint].tag == PROTOTYPE_TERM_VAR &&
				terms->terms[right_endpoint].as.var.binding_id ==
					type->right_endpoint_binding_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id == type->endpoint_context_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject == type->relation_type_term_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier == prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY:
		{
				const struct prototype_substitution* source =
					prototype_substitution_get(
						substitutions,
						request->key.substitution.source.substitution_id
					);
			const struct prototype_hott_bridge* source_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.source_bridge_id
				);
			const struct prototype_hott_bridge* target_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.target_bridge_id
				);
			const struct prototype_hott_substitution_action_certificate* action =
				&certificate->data.substitution;
			const struct prototype_substitution* result =
				prototype_substitution_get(
					substitutions, action->result_substitution_id
				);
				struct prototype_term_conversion_result left;
				struct prototype_term_conversion_result right;
				struct prototype_certified_substitution_ref certified_result;
				if (request->kind != PROTOTYPE_HOTT_ACTION_SUBSTITUTION || !source ||
					!source_bridge || !target_bridge || !result ||
					prototype_cwf_certified_substitution_ref_get(
						cwf_certificates,
						substitutions,
						judgement,
						action->result_substitution_id,
						action->result_substitution_certificate_id,
						&certified_result
					) != 0 ||
				result->source_context != source_bridge->bridge_context_id ||
				result->target_context != target_bridge->bridge_context_id ||
				action->normalization_profile !=
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
				action->term_graph_revision > terms->term_count ||
				prototype_substitution_compare_pointwise(
					(struct prototype_substitution_db*)substitutions,
					contexts,
					terms,
					type_declarations,
					action->left_naturality_lhs_substitution_id,
					action->left_naturality_rhs_substitution_id,
					action->normalization_profile,
					action->step_limit,
					&left
				) != 0 || left.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
				prototype_substitution_compare_pointwise(
					(struct prototype_substitution_db*)substitutions,
					contexts,
					terms,
					type_declarations,
					action->right_naturality_lhs_substitution_id,
					action->right_naturality_rhs_substitution_id,
					action->normalization_profile,
					action->step_limit,
					&right
				) != 0 || right.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			return 1;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION:
		{
			const struct prototype_hott_identity_type_computation_certificate*
				identity = &certificate->data.identity_type;
			const struct prototype_judgement_proposition* source =
				prototype_judgement_claim_proposition(
					judgement, request->key.identity_type.source_claim_id
				);
				const struct prototype_hott_bridge* bridge =
					prototype_hott_bridge_db_get(
						bridges, request->key.identity_type.source_bridge_id
					);
				const struct prototype_hott_bridge_certificate* bridge_certificate =
					bridge ? &bridges->certificates[bridge->id] : NULL;
				const struct prototype_judgement_proposition* identity_is_type =
					prototype_judgement_claim_proposition(
						judgement, identity->identity_type_is_type_claim_id
					);
				if (request->kind !=
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
					!source || source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
					!bridge || !bridge_certificate ||
					(bridge_certificate->semantics !=
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
					 bridge_certificate->semantics !=
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) ||
					bridge->source_context_id != source->context_id ||
					!identity_is_type || identity_is_type->kind !=
						PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
					identity_is_type->context_id != identity->endpoint_context_id ||
					identity_is_type->subject != identity->identity_type_term_id ||
					identity_is_type->classifier != source->classifier) {
					return 0;
				}
				const struct prototype_context* endpoint_context =
					prototype_context_get(contexts, identity->endpoint_context_id);
				const struct prototype_cwf_certificate* left_context_certificate =
					prototype_cwf_certificate_db_get_kind(
						cwf_certificates,
						identity->left_context_certificate_id,
						PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION
					);
				const struct prototype_cwf_certificate* right_context_certificate =
					prototype_cwf_certificate_db_get_kind(
						cwf_certificates,
						identity->right_context_certificate_id,
						PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION
					);
					if (!endpoint_context || !left_context_certificate ||
					!right_context_certificate ||
					left_context_certificate->structural_id != endpoint_context->parent ||
					right_context_certificate->structural_id !=
						identity->endpoint_context_id) {
						return 0;
					}
					if (identity->computation_rule ==
						PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER) {
						const struct prototype_context* source_context =
							prototype_context_get(contexts, source->context_id);
						uint32_t source_has_type_claim_id;
						int source_premise_status =
							hott_find_has_type_for_is_type_readonly_depth(
								judgement,
								request->key.identity_type.source_claim_id,
								(uint32_t)judgement->claim_count + 1,
								&source_has_type_claim_id
							);
						const struct prototype_hott_action_request* object_request = NULL;
						const struct prototype_hott_action_certificate*
							object_certificate = NULL;
						const struct prototype_hott_action_certificate* universe_action = NULL;
						for (uint32_t i = 0; source_premise_status == 0 &&
								i < db->result_count; ++i) {
							const struct prototype_hott_action_result* result = &db->results[i];
							const struct prototype_hott_action_request* candidate =
								prototype_hott_action_request_get(db, result->request_id);
							if (!candidate || candidate->kind !=
									PROTOTYPE_HOTT_ACTION_OBJECT_TERM ||
								candidate->key.object_term.source_claim_id !=
									source_has_type_claim_id ||
								candidate->key.object_term.source_bridge_id != bridge->id ||
								result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
								result->certificate_id >= db->certificate_count ||
								db->certificates[result->certificate_id].kind !=
									PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
								continue;
							}
							object_request = candidate;
							object_certificate =
								&db->certificates[result->certificate_id];
							uint32_t universe_result_id;
							if (hott_action_result_for_request(
									db,
									candidate->key.object_term.
										identity_type_action_request_id,
									&universe_result_id
								) == 0) {
								const struct prototype_hott_action_result* universe_result =
									prototype_hott_action_result_get(
										db, universe_result_id
									);
								universe_action = universe_result &&
									universe_result->certificate_id < db->certificate_count ?
									&db->certificates[universe_result->certificate_id] : NULL;
							}
							break;
						}
						const struct prototype_judgement_proposition* universe_witness =
							object_certificate ?
							prototype_judgement_claim_proposition(
								judgement,
								object_certificate->data.object_term.
									witness_has_type_claim_id
							) : NULL;
						const struct prototype_judgement_proposition* relation_claim =
							prototype_judgement_claim_proposition(
								judgement,
								identity->backing_type_former_has_type_claim_id
							);
						const struct prototype_judgement_proposition* identity_claim =
							prototype_judgement_claim_proposition(
								judgement, identity->identity_type_has_type_claim_id
							);
						const struct prototype_term* relation =
							identity->backing_type_former_term_id < terms->term_count ?
							&terms->terms[identity->backing_type_former_term_id] : NULL;
						const struct prototype_term* identity_term =
							identity->identity_type_term_id < terms->term_count ?
							&terms->terms[identity->identity_type_term_id] : NULL;
						uint32_t endpoint_path[2];
						uint32_t endpoint_count;
						uint32_t expected_left_type;
						uint32_t expected_right_type;
						if (source_premise_status != 0 || !source_context ||
							!object_request || !object_certificate ||
							!universe_action || !universe_witness ||
							!relation_claim || !identity_claim || !relation ||
							!identity_term || source->subject >= terms->term_count ||
							source->classifier >=
								terms->term_count || terms->terms[source->classifier].tag !=
								PROTOTYPE_TERM_UNIVERSE_VAR ||
							universe_action->kind !=
								PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
							(universe_action->data.identity_type.computation_rule !=
								PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
							 universe_action->data.identity_type.computation_rule !=
								PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
							identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_left_input_binding_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_right_input_binding_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_input_identity_binding_id != PROTOTYPE_INVALID_ID ||
							prototype_context_extension_path(
								contexts,
								bridge->bridge_context_id,
								identity->endpoint_context_id,
								endpoint_path,
								2,
								&endpoint_count
							) != 0 || endpoint_count != 2 ||
							prototype_term_reindex(
								terms,
								type_declarations,
								contexts,
								(struct prototype_substitution_db*)substitutions,
								source->subject,
								bridge->left_substitution_id,
								&expected_left_type
							) != 0 || prototype_term_reindex(
								terms,
								type_declarations,
								contexts,
								(struct prototype_substitution_db*)substitutions,
								source->subject,
								bridge->right_substitution_id,
								&expected_right_type
							) != 0 || prototype_context_classifier_term(
								prototype_context_get(contexts, endpoint_path[0])
							) != expected_left_type || prototype_context_classifier_term(
								prototype_context_get(contexts, endpoint_path[1])
							) != expected_right_type || identity->left_endpoint_binding_id !=
								prototype_context_get(contexts, endpoint_path[0])->binding_id ||
							identity->right_endpoint_binding_id !=
								prototype_context_get(contexts, endpoint_path[1])->binding_id ||
							relation_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
							relation_claim->context_id != bridge->bridge_context_id ||
							relation_claim->subject != identity->backing_type_former_term_id ||
							identity_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
							identity_claim->context_id != identity->endpoint_context_id ||
							identity_claim->subject != identity->identity_type_term_id ||
							identity_claim->classifier != source->classifier ||
							identity_term->tag != PROTOTYPE_TERM_APP) {
							return 0;
						}
						const struct prototype_term* relation_at_left =
							identity_term->as.app.function < terms->term_count ? &terms->terms[
								identity_term->as.app.function
							] : NULL;
						if (!hott_universe_projection_claim_is_valid(
								judgement,
								terms,
								type_declarations,
								identity->backing_type_former_has_type_claim_id,
								object_certificate->data.object_term.
									witness_has_type_claim_id,
								expected_left_type,
								expected_right_type,
								PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION
							) || !relation_at_left || relation_at_left->tag !=
								PROTOTYPE_TERM_APP || relation_at_left->as.app.function !=
								identity->backing_type_former_term_id ||
							identity_term->as.app.argument >= terms->term_count ||
							terms->terms[identity_term->as.app.argument].tag !=
								PROTOTYPE_TERM_VAR || terms->terms[
									identity_term->as.app.argument
								].as.var.binding_id != identity->right_endpoint_binding_id ||
							relation_at_left->as.app.argument >= terms->term_count ||
							terms->terms[relation_at_left->as.app.argument].tag !=
								PROTOTYPE_TERM_VAR || terms->terms[
									relation_at_left->as.app.argument
								].as.var.binding_id != identity->left_endpoint_binding_id) {
							return 0;
						}
						return 1;
					}
					if (identity->computation_rule ==
							PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT ||
						(identity->computation_rule ==
							PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
						 source->context_id != prototype_context_empty(contexts) &&
						 source->subject < terms->term_count &&
						 terms->terms[source->subject].tag ==
							PROTOTYPE_TERM_UNIVERSE_VAR)) {
					const struct prototype_type_declaration* generated =
						identity->generated_type_declaration_id <
							type_declarations->type_count ?
						&type_declarations->type_declarations[
							identity->generated_type_declaration_id
						] : NULL;
					const struct prototype_judgement_proposition* former_claim =
						prototype_judgement_claim_proposition(
							judgement,
							identity->backing_type_former_has_type_claim_id
						);
					const struct prototype_judgement_proposition* family_claim =
						prototype_judgement_claim_proposition(
							judgement, identity->identity_type_has_type_claim_id
						);
					uint32_t endpoint_path[2];
					uint32_t endpoint_count;
					uint32_t former_type_id;
					uint32_t former_arguments[1];
					uint32_t former_argument_count;
					uint32_t identity_type_id;
					uint32_t identity_arguments[2];
					uint32_t identity_argument_count;
					uint32_t left_endpoint;
					uint32_t right_endpoint;
					if (source->context_id == prototype_context_empty(contexts) ||
						bridges->certificates[bridge->id].semantics !=
							PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
						!hott_term_is_constant_over_context(
							terms, contexts, source->context_id, source->subject
						) || prototype_context_extension_path(
							contexts,
							bridge->bridge_context_id,
							identity->endpoint_context_id,
							endpoint_path,
							2,
							&endpoint_count
						) != 0 || endpoint_count != 2 ||
						prototype_context_classifier_term(
							prototype_context_get(contexts, endpoint_path[0])
						) != source->subject || prototype_context_classifier_term(
							prototype_context_get(contexts, endpoint_path[1])
						) != source->subject || identity->left_endpoint_binding_id !=
							prototype_context_get(contexts, endpoint_path[0])->binding_id ||
						identity->right_endpoint_binding_id !=
							prototype_context_get(contexts, endpoint_path[1])->binding_id ||
						prototype_term_var(
							terms,
							identity->left_endpoint_binding_id,
							&left_endpoint
						) != 0 || prototype_term_var(
							terms,
							identity->right_endpoint_binding_id,
							&right_endpoint
						) != 0 || !family_claim || family_claim->kind !=
							PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						family_claim->context_id != identity->endpoint_context_id ||
						family_claim->subject != identity->identity_type_term_id ||
						family_claim->classifier != source->classifier) {
						return 0;
					}
					if (!generated) {
						uint32_t expected_identity;
						uint32_t computation = source->subject < terms->term_count &&
							terms->terms[source->subject].tag ==
								PROTOTYPE_TERM_THUNK_TYPE ?
							terms->terms[source->subject].as.thunk_type.computation :
							PROTOTYPE_INVALID_ID;
						if (computation >= terms->term_count ||
							terms->terms[computation].tag != PROTOTYPE_TERM_PI ||
							identity->generated_type_declaration_id !=
								PROTOTYPE_INVALID_ID ||
							identity->backing_type_former_term_id !=
								PROTOTYPE_INVALID_ID ||
							identity->backing_type_former_has_type_claim_id !=
								PROTOTYPE_INVALID_ID ||
							hott_build_nondependent_pi_identity_type(
								(struct prototype_term_db*)terms,
								(struct prototype_type_declaration_db*)type_declarations,
								prototype_context_empty(contexts),
								source->subject,
								identity->left_endpoint_binding_id,
								identity->right_endpoint_binding_id,
								identity->pointwise_left_input_binding_id,
								identity->pointwise_right_input_binding_id,
								identity->pointwise_input_identity_binding_id,
								&expected_identity
							) != 0 || expected_identity !=
								identity->identity_type_term_id) {
							return 0;
						}
						return 1;
					}
					if (generated->origin_kind !=
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						generated->origin_source_carrier_term_id != source->subject ||
						prototype_term_type_instance_info(
							terms,
							identity->backing_type_former_term_id,
							&former_type_id,
							former_arguments,
							&former_argument_count
						) != 0 || former_type_id !=
							identity->generated_type_declaration_id ||
						former_argument_count != 0 || !former_claim ||
						former_claim->context_id != bridge->bridge_context_id ||
						former_claim->subject != identity->backing_type_former_term_id ||
						prototype_term_type_instance_info(
							terms,
							identity->identity_type_term_id,
							&identity_type_id,
							identity_arguments,
							&identity_argument_count
						) != 0 || identity_type_id !=
							identity->generated_type_declaration_id ||
						identity_argument_count != 2 ||
						identity_arguments[0] != left_endpoint ||
						identity_arguments[1] != right_endpoint ||
						!prototype_type_declaration_validate_generated_identity(
							terms,
							type_declarations,
							contexts,
							source->subject,
							identity->generated_type_declaration_id,
							prototype_type_declaration_generated_identity_rule_for_source(
								terms,
								source->subject
							)
						)) {
						return 0;
					}
					return 1;
				}
				if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT) {
					const struct prototype_type_declaration* generated =
						identity->generated_type_declaration_id <
							type_declarations->type_count ?
						&type_declarations->type_declarations[
							identity->generated_type_declaration_id
						] : NULL;
					uint32_t source_type_id;
					uint32_t source_arguments[2];
					uint32_t source_argument_count;
					uint32_t origin_type_id;
					uint32_t origin_arguments[1];
					uint32_t origin_argument_count;
					uint32_t identity_type_id;
					uint32_t boundary[8];
					uint32_t boundary_count;
					uint32_t endpoint_path[2];
					uint32_t endpoint_count;
					uint32_t expected[8];
					const struct prototype_hott_bridge* parent =
						prototype_hott_bridge_db_get(
							bridges, bridge_certificate->parent_bridge_id
						);
					const struct prototype_judgement_proposition* former_claim =
						prototype_judgement_claim_proposition(
							judgement,
							identity->backing_type_former_has_type_claim_id
						);
					const struct prototype_judgement_proposition* family_claim =
						prototype_judgement_claim_proposition(
							judgement, identity->identity_type_has_type_claim_id
						);
					if (source->context_id == prototype_context_empty(contexts) ||
						bridge_certificate->semantics !=
							PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
						!parent || !generated || generated->origin_kind !=
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						generated->parameter_context !=
							prototype_context_empty(contexts) ||
						generated->index_count != 8 ||
						generated->constructor_count == 0 ||
						prototype_term_type_instance_info(
							terms,
							source->subject,
							&source_type_id,
							source_arguments,
							&source_argument_count
						) != 0 || source_argument_count != 2 || source_type_id >=
							type_declarations->type_count || type_declarations->
							type_declarations[source_type_id].origin_kind !=
								PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						prototype_term_type_instance_info(
							terms,
							generated->origin_source_carrier_term_id,
							&origin_type_id,
							origin_arguments,
							&origin_argument_count
						) != 0 || origin_type_id != source_type_id ||
						origin_argument_count != 0 ||
						prototype_context_extension_path(
							contexts,
							bridge->bridge_context_id,
							identity->endpoint_context_id,
							endpoint_path,
							2,
							&endpoint_count
						) != 0 || endpoint_count != 2 ||
						prototype_term_type_instance_info(
							terms,
							identity->identity_type_term_id,
							&identity_type_id,
							boundary,
							&boundary_count
						) != 0 || identity_type_id !=
							identity->generated_type_declaration_id ||
						boundary_count != 8 || !former_claim || !family_claim ||
						former_claim->context_id != bridge->bridge_context_id ||
						former_claim->subject !=
							identity->backing_type_former_term_id ||
						family_claim->context_id != identity->endpoint_context_id ||
						family_claim->subject != identity->identity_type_term_id ||
						prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[0],
							bridge->left_substitution_id,
							&expected[0]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[1],
							bridge->left_substitution_id,
							&expected[1]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							identity->left_endpoint_binding_id,
							&expected[2]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[0],
							bridge->right_substitution_id,
							&expected[3]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[1],
							bridge->right_substitution_id,
							&expected[4]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							identity->right_endpoint_binding_id,
							&expected[5]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							prototype_context_get(
								contexts, parent->bridge_context_id
							)->binding_id,
							&expected[6]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							prototype_context_get(
								contexts, bridge->bridge_context_id
							)->binding_id,
							&expected[7]
						) != 0 || memcmp(boundary, expected, sizeof(boundary)) != 0) {
						return 0;
					}
					return 1;
				}
				if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
				uint32_t endpoint_path[2];
				uint32_t endpoint_count;
				const struct prototype_judgement_proposition* identity_claim =
					prototype_judgement_claim_proposition(
						judgement, identity->identity_type_has_type_claim_id
					);
				uint32_t expected_identity;
				int source_is_computation_pi = source->subject < terms->term_count &&
					terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK_TYPE &&
					terms->terms[source->subject].as.thunk_type.computation <
						terms->term_count && terms->terms[
						terms->terms[source->subject].as.thunk_type.computation
					].tag == PROTOTYPE_TERM_PI;
				int source_is_type_pi = source->subject < terms->term_count &&
					terms->terms[source->subject].tag == PROTOTYPE_TERM_PI;
				if (identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
					identity->backing_type_former_term_id != PROTOTYPE_INVALID_ID ||
					identity->backing_type_former_has_type_claim_id !=
						PROTOTYPE_INVALID_ID ||
					(!source_is_computation_pi && !source_is_type_pi) ||
					prototype_context_extension_path(
						contexts,
						source->context_id,
						identity->endpoint_context_id,
						endpoint_path,
						2,
						&endpoint_count
					) != 0 || endpoint_count != 2 ||
					prototype_context_classifier_term(
						prototype_context_get(contexts, endpoint_path[0])
					) != source->subject || prototype_context_classifier_term(
						prototype_context_get(contexts, endpoint_path[1])
					) != source->subject || identity->left_endpoint_binding_id !=
						prototype_context_get(contexts, endpoint_path[0])->binding_id ||
					identity->right_endpoint_binding_id !=
						prototype_context_get(contexts, endpoint_path[1])->binding_id ||
					hott_build_nondependent_pi_identity_type(
						(struct prototype_term_db*)terms,
						(struct prototype_type_declaration_db*)type_declarations,
						source->context_id,
						source->subject,
						identity->left_endpoint_binding_id,
						identity->right_endpoint_binding_id,
						identity->pointwise_left_input_binding_id,
						identity->pointwise_right_input_binding_id,
						identity->pointwise_input_identity_binding_id,
						&expected_identity
					) != 0 || expected_identity != identity->identity_type_term_id ||
					!identity_claim || identity_claim->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					identity_claim->context_id != identity->endpoint_context_id ||
					identity_claim->subject != identity->identity_type_term_id ||
					identity_claim->classifier != source->classifier) {
					return 0;
				}
				for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
					if (judgement->derivations[i].conclusion_claim_id ==
							identity->identity_type_has_type_claim_id &&
						judgement->derivations[i].proof_kind ==
							(source_is_computation_pi ?
							 PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION :
							 PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO)) {
						return 1;
					}
				}
				return 0;
			}
			const struct prototype_type_declaration* generated =
				identity->generated_type_declaration_id <
					type_declarations->type_count ?
				&type_declarations->type_declarations[
					identity->generated_type_declaration_id
				] : NULL;
			const struct prototype_judgement_proposition* former_claim =
				prototype_judgement_claim_proposition(
					judgement, identity->backing_type_former_has_type_claim_id
				);
			const struct prototype_judgement_proposition* identity_type_claim =
				prototype_judgement_claim_proposition(
					judgement, identity->identity_type_has_type_claim_id
				);
			uint32_t former_type_id;
			uint32_t former_args[16];
			uint32_t former_arg_count;
			uint32_t identity_type_id;
			uint32_t identity_type_args[16];
			uint32_t identity_type_arg_count;
			uint32_t index_path[2];
			uint32_t index_count;
			if ((identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
				identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
				identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) ||
				identity->pointwise_left_input_binding_id != PROTOTYPE_INVALID_ID ||
				identity->pointwise_right_input_binding_id != PROTOTYPE_INVALID_ID ||
				identity->pointwise_input_identity_binding_id != PROTOTYPE_INVALID_ID ||
				!generated || generated->origin_kind !=
					PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
				generated->origin_source_carrier_term_id != source->subject ||
				generated->parameter_context != source->context_id ||
				generated->parameter_count != 0 || generated->index_count != 2 ||
				prototype_context_extension_path(
					contexts,
					generated->parameter_context,
					generated->index_context,
					index_path,
					2,
					&index_count
				) != 0 || index_count != 2 ||
				prototype_context_classifier_term(
					prototype_context_get(contexts, index_path[0])
				) != source->subject ||
				prototype_context_classifier_term(
					prototype_context_get(contexts, index_path[1])
				) != source->subject ||
				identity->endpoint_context_id != generated->index_context ||
				identity->left_endpoint_binding_id !=
					prototype_context_get(contexts, index_path[0])->binding_id ||
				identity->right_endpoint_binding_id !=
					prototype_context_get(contexts, index_path[1])->binding_id ||
				prototype_term_type_instance_info(
					terms,
					identity->backing_type_former_term_id,
					&former_type_id,
					former_args,
					&former_arg_count
				) != 0 || former_type_id !=
					identity->generated_type_declaration_id ||
				former_arg_count != 0 || !former_claim ||
				former_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				former_claim->context_id != source->context_id ||
				former_claim->subject != identity->backing_type_former_term_id ||
				former_claim->classifier != generated->formation_classifier ||
				prototype_term_type_instance_info(
					terms,
					identity->identity_type_term_id,
					&identity_type_id,
					identity_type_args,
					&identity_type_arg_count
				) != 0 || identity_type_id !=
					identity->generated_type_declaration_id ||
				identity_type_arg_count != 2 || !identity_type_claim ||
				identity_type_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				identity_type_claim->context_id != identity->endpoint_context_id ||
				identity_type_claim->subject != identity->identity_type_term_id ||
				identity_type_claim->classifier != source->classifier) {
				return 0;
			}
			const struct prototype_context* left_index =
				prototype_context_get(contexts, index_path[0]);
			const struct prototype_context* right_index =
				prototype_context_get(contexts, index_path[1]);
			uint32_t right_family;
			uint32_t right_pi;
			uint32_t left_family;
			uint32_t expected_formation;
			if (!left_index || !right_index || prototype_term_pure_family(
					terms,
					right_index->binding_id,
					source->classifier,
					&right_family
				) != 0 || prototype_term_pi_family(
					terms, source->subject, right_family, &right_pi
				) != 0 || prototype_term_pure_family(
					terms,
					left_index->binding_id,
					right_pi,
					&left_family
				) != 0 || prototype_term_pi_family(
					terms, source->subject, left_family, &expected_formation
				) != 0 || expected_formation != generated->formation_classifier) {
				return 0;
			}
			int formation_derivation = 0;
			for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
				const struct prototype_judgement_derivation* derivation =
					&judgement->derivations[i];
				if (derivation->conclusion_claim_id ==
						identity->backing_type_former_has_type_claim_id &&
					derivation->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO) {
					formation_derivation = 1;
					break;
				}
			}
			uint32_t left_endpoint;
			uint32_t right_endpoint;
			if (!formation_derivation || generated->constructor_count == 0 ||
				prototype_term_var(
					terms, identity->left_endpoint_binding_id, &left_endpoint
				) != 0 || prototype_term_var(
					terms, identity->right_endpoint_binding_id, &right_endpoint
				) != 0 || identity_type_args[0] != left_endpoint ||
				identity_type_args[1] != right_endpoint) {
				return 0;
			}
			if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
				return prototype_type_declaration_validate_generated_identity(
					terms,
					type_declarations,
					contexts,
					source->subject,
					identity->generated_type_declaration_id,
					identity->computation_rule
				);
			}
			return prototype_type_declaration_validate_generated_identity(
				terms,
				type_declarations,
				contexts,
				source->subject,
				identity->generated_type_declaration_id,
				identity->computation_rule
			);
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.term.source_bridge_id
				);
			const struct prototype_hott_action_request* type_request =
				prototype_hott_action_request_get(
					db, request->key.term.relation_type_action_request_id
				);
			const struct prototype_hott_action_certificate* type_certificate = NULL;
			for (uint32_t i = 0; i < db->result_count; ++i) {
				const struct prototype_hott_action_result* result = &db->results[i];
				if (result->request_id == request->key.term.relation_type_action_request_id &&
					result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
					result->certificate_id < db->certificate_count) {
					type_certificate = &db->certificates[result->certificate_id];
					break;
				}
			}
			const struct prototype_hott_term_action_certificate* term =
				&certificate->data.term;
			const struct prototype_substitution* endpoint =
				prototype_substitution_get(
					substitutions, term->endpoint_instantiation_substitution_id
				);
			const struct prototype_cwf_certificate* left_extension =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					term->left_endpoint_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_cwf_certificate* right_extension =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					term->right_endpoint_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_judgement_claim* witness_claim =
				prototype_judgement_claim_get(
					judgement, term->witness_has_type_claim_id
				);
			const struct prototype_judgement_claim* left_endpoint_claim =
				left_extension ? prototype_judgement_claim_get(
					judgement, left_extension->claim_id
				) : NULL;
			const struct prototype_judgement_claim* right_endpoint_claim =
				right_extension ? prototype_judgement_claim_get(
					judgement, right_extension->claim_id
				) : NULL;
			if (request->kind != PROTOTYPE_HOTT_ACTION_TERM || !source || !bridge ||
				!type_request || !type_certificate ||
				type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE ||
				!endpoint || !left_extension || !right_extension || !witness_claim ||
				!left_endpoint_claim || !right_endpoint_claim ||
				endpoint->source_context != bridge->bridge_context_id ||
				endpoint->target_context !=
					type_certificate->data.relation_type.endpoint_context_id ||
				endpoint->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
				left_extension->structural_id != endpoint->first ||
				right_extension->structural_id !=
					term->endpoint_instantiation_substitution_id ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->context_id != bridge->bridge_context_id ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->subject != term->witness_term_id) {
				return 0;
			}
			uint32_t relation;
			return prototype_term_reindex(
				(struct prototype_term_db*)terms,
				type_declarations,
				contexts,
				(struct prototype_substitution_db*)substitutions,
				type_certificate->data.relation_type.relation_type_term_id,
				term->endpoint_instantiation_substitution_id,
				&relation
			) == 0 && relation == prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM:
		{
			const struct prototype_hott_object_term_action_certificate* object =
				&certificate->data.object_term;
			const struct prototype_judgement_proposition* source =
				prototype_judgement_claim_proposition(
					judgement, request->key.object_term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.object_term.source_bridge_id
				);
			const struct prototype_hott_action_request* identity_request =
				prototype_hott_action_request_get(
					db, request->key.object_term.identity_type_action_request_id
				);
			const struct prototype_hott_action_certificate* identity_certificate = NULL;
			for (uint32_t i = 0; i < db->result_count; ++i) {
				const struct prototype_hott_action_result* result = &db->results[i];
				if (result->request_id ==
						request->key.object_term.identity_type_action_request_id &&
					result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
					result->certificate_id < db->certificate_count) {
					identity_certificate = &db->certificates[result->certificate_id];
					break;
				}
			}
			const struct prototype_judgement_proposition* identity_source =
				identity_request && identity_request->kind ==
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
				prototype_judgement_claim_proposition(
					judgement,
					identity_request->key.identity_type.source_claim_id
				) : NULL;
			const struct prototype_judgement_proposition* left =
				prototype_judgement_claim_proposition(
					judgement, object->left_endpoint_claim_id
				);
			const struct prototype_judgement_proposition* right =
				prototype_judgement_claim_proposition(
					judgement, object->right_endpoint_claim_id
				);
			const struct prototype_judgement_proposition* family =
				prototype_judgement_claim_proposition(
					judgement, object->identity_family_has_type_claim_id
				);
			const struct prototype_judgement_proposition* witness =
				prototype_judgement_claim_proposition(
					judgement, object->witness_has_type_claim_id
				);
			if (request->kind != PROTOTYPE_HOTT_ACTION_OBJECT_TERM || !source ||
				!bridge || !identity_request || !identity_certificate ||
				identity_certificate->kind !=
					PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
				!identity_source || !left || !right || !family || !witness ||
				source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				identity_source->subject != source->classifier ||
				left->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				right->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				left->context_id != bridge->bridge_context_id ||
				right->context_id != bridge->bridge_context_id ||
				family->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				family->context_id != bridge->bridge_context_id ||
				family->classifier != identity_source->classifier ||
				witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				witness->context_id != bridge->bridge_context_id ||
				witness->subject != object->witness_term_id ||
				witness->classifier != family->subject) {
				return 0;
			}
			uint32_t expected_left;
			uint32_t expected_right;
			uint32_t expected_left_classifier;
			uint32_t expected_right_classifier;
			if (prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->subject,
					bridge->left_substitution_id,
					&expected_left
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->subject,
					bridge->right_substitution_id,
					&expected_right
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->classifier,
					bridge->left_substitution_id,
					&expected_left_classifier
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->classifier,
					bridge->right_substitution_id,
					&expected_right_classifier
				) != 0 || left->subject != expected_left ||
				left->classifier != expected_left_classifier ||
				right->subject != expected_right ||
				right->classifier != expected_right_classifier) {
				return 0;
			}
			uint32_t expected_family = identity_certificate->data.identity_type.
				identity_type_term_id;
			if (prototype_term_graph_substitute_bound_var(
					(struct prototype_term_db*)terms,
					type_declarations,
					expected_family,
					identity_certificate->data.identity_type.left_endpoint_binding_id,
					left->subject,
					&expected_family
				) != 0 || prototype_term_graph_substitute_bound_var(
					(struct prototype_term_db*)terms,
					type_declarations,
					expected_family,
					identity_certificate->data.identity_type.right_endpoint_binding_id,
					right->subject,
					&expected_family
				) != 0) {
				return 0;
			}
			return family->subject == expected_family;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_certificate_add(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_certificate certificate,
	uint32_t* p_certificate_id
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || !p_certificate_id || !db->certificates ||
		db->certificate_count >= db->certificate_capacity ||
		prototype_kernel_view_validate_stores(kernel) != 0) {
		return -1;
	}
	certificate.id = (uint32_t)db->certificate_count;
	if (!hott_action_certificate_is_valid(
			db,
			&certificate,
			certificate.id,
			contexts,
			substitutions,
			cwf_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].request_id == certificate.request_id) {
			return -1;
		}
	}
	db->certificates[db->certificate_count++] = certificate;
	*p_certificate_id = certificate.id;
	return 0;
}

int prototype_hott_action_result_publish(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_result result,
	uint32_t* p_result_id
) {
	if (db) {
		db->outcome_publish_requests++;
	}
	if (!db || !p_result_id || !db->results ||
		db->result_count >= db->result_capacity ||
		!prototype_hott_action_request_get(db, result.request_id) ||
		!hott_deterministic_outcome_is_valid(&result.outcome, 0)) {
		return -1;
	}
	if (result.outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		if (result.outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE ||
			result.certificate_id >= db->certificate_count ||
			db->certificates[result.certificate_id].request_id !=
				result.request_id) {
			return -1;
		}
	} else if (!hott_residual_reason_is_valid(result.outcome.residual_reason) ||
		result.outcome.residual_reason == PROTOTYPE_HOTT_RESIDUAL_NONE ||
		result.certificate_id != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		if (db->results[i].request_id == result.request_id) {
			return -1;
		}
	}
	result.id = (uint32_t)db->result_count;
	db->results[db->result_count++] = result;
	*p_result_id = result.id;
	return 0;
}

static int hott_action_result_for_request(
	const struct prototype_hott_action_db* db,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	if (!db || !p_result_id) {
		return -1;
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		if (db->results[i].request_id == request_id) {
			*p_result_id = i;
			return 0;
		}
	}
	return 1;
}
