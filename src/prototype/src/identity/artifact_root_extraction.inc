int prototype_hott_register_identity_root(
	struct prototype_artifact_interface* interface,
	const struct prototype_hott_action_db* actions,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t witness_has_type_claim_id,
	uint32_t* p_root_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	const struct prototype_hott_bridge* bridge = request && bridges &&
		request->kind == PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_judgement_proposition* source = request && judgement &&
		request->kind == PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_hott_identity_type_computation_certificate* identity =
		certificate ? &certificate->data.identity_type : NULL;
	const struct prototype_judgement_proposition* family = identity && judgement ?
		prototype_judgement_claim_proposition(
			judgement, identity->identity_type_has_type_claim_id
		) : NULL;
	if (!interface || !actions || !kernel || !bridges || !p_root_id ||
		!result || !request || !certificate || !judgement || !contexts || !terms ||
		!bridge || !source || !identity || !family ||
		prototype_hott_action_db_validate(actions, kernel, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		certificate->request_id != request->id || result->request_id != request->id ||
		bridge->source_context_id != prototype_context_empty(contexts) ||
		source->context_id != bridge->source_context_id ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		family->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		family->context_id != identity->endpoint_context_id ||
		family->subject != identity->identity_type_term_id ||
		family->classifier != source->classifier ||
		(identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) ||
		hott_term_has_compiler_relation_head(
			terms, family->subject
		) != 0) {
		return -1;
	}
	if (witness_has_type_claim_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, witness_has_type_claim_id
			);
		if (!witness || witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			witness->context_id != family->context_id ||
			witness->classifier != family->subject ||
			hott_term_has_compiler_relation_head(
				terms, witness->subject
			) != 0) {
			return -1;
		}
	}
	return prototype_artifact_interface_add_identity_root(
		interface,
		terms,
		kernel->type_declarations,
		kernel->contexts,
		judgement,
		request->key.identity_type.source_claim_id,
		identity->identity_type_has_type_claim_id,
		witness_has_type_claim_id,
		identity->computation_rule,
		p_root_id
	);
}

