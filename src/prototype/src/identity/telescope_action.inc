static int hott_zero_field_ordinary_adt(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t type_term
) {
	uint32_t type_id;
	const struct prototype_type_declaration* type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			type_term,
			&type_id,
			&type
		) != 0 || type_id >= type_declarations->type_count) {
		return 0;
	}
	if (type->parameter_count != 0 || type->constructor_count == 0) {
		return 0;
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		uint32_t constructor_id = type->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		const struct prototype_context* parameters = prototype_context_get(
			contexts, constructor->parameter_context
		);
		const struct prototype_context* fields = prototype_context_get(
			contexts, constructor->field_context
		);
		if (!parameters || !fields || parameters->depth != fields->depth) {
			return 0;
		}
	}
	return 1;
}

static int hott_ordinary_adt_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type
) {
	if (!terms || !type_declarations || !contexts || !source_type ||
		source_type->parameter_count != 0 || source_type->constructor_count == 0) {
		return 0;
	}
	uint32_t source_type_id;
	uint32_t source_arguments[16];
	uint32_t source_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			source_carrier,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_type_id != source_type->type_index ||
		source_argument_count != source_type->index_count) {
		return 0;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t constructor_id = source_type->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t field_path[64];
		uint32_t field_count;
		if (constructor->parameter_context != source_type->parameter_context ||
			prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				field_path,
				64,
				&field_count
			) != 0) {
			return 0;
		}
		if (source_type->index_count > 0 && field_count > 0) {
			return 0;
		}
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field =
				prototype_context_get(contexts, field_path[j]);
			if (!field) {
				return 0;
			}
			uint32_t field_classifier =
				prototype_context_classifier_term(field);
			int dependent = 0;
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous =
					prototype_context_get(contexts, field_path[k]);
				if (!previous) {
					return 0;
				}
				dependent = dependent || prototype_term_contains_free_binding(
					terms, field_classifier, previous->binding_id
				);
			}
			if (dependent) {
				/* The Context identity action selects this fiber from the
				 * identities of the preceding fields. */
				continue;
			}
			if (field_classifier != source_carrier) {
				uint32_t field_type_id;
				const struct prototype_type_declaration* field_type;
				if (prototype_type_view_declaration_query(
						type_declarations,
						contexts,
						terms,
						field_classifier,
						&field_type_id,
						&field_type
					) != 0 || !field_type || field_type_id >=
						type_declarations->type_count ||
					field_type->parameter_count != 0 ||
					field_type->constructor_count == 0) {
					return 0;
				}
			}
		}
	}
	(void)source_arguments;
	return 1;
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t parameter_context,
	uint32_t thunk_type
);

static int hott_thunk_return_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t thunk_type
) {
	if (!terms || !type_declarations || thunk_type >= terms->term_count ||
		terms->terms[thunk_type].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t computation = terms->terms[thunk_type].as.thunk_type.computation;
	int supported = computation < terms->term_count &&
		terms->terms[computation].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		prototype_term_effect_row_purity(
			terms, terms->terms[computation].as.computation_type.label
		) == PROTOTYPE_EFFECT_ROW_PURITY_PURE;
	(void)type_declarations;
	(void)parameter_context;
	return supported;
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t parameter_context,
	uint32_t thunk_type
) {
	if (!terms || !type_declarations || !contexts ||
		thunk_type >= terms->term_count ||
		terms->terms[thunk_type].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t pi = terms->terms[thunk_type].as.thunk_type.computation;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	uint32_t codomain_binding;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_contains_free_binding(
			terms, codomain, codomain_binding
		) || prototype_term_effect_row_purity(
			terms, terms->terms[codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t domain_type_id;
	const struct prototype_type_declaration* domain_type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			terms->terms[pi].as.pi.domain,
			&domain_type_id,
			&domain_type
		) != 0 || !domain_type || !hott_ordinary_adt_identity_supported(
			terms,
			type_declarations,
			contexts,
			terms->terms[pi].as.pi.domain,
			domain_type
		)) {
		return 0;
	}
	uint32_t result_type = terms->terms[codomain].as.computation_type.result;
	uint32_t result_type_id;
	const struct prototype_type_declaration* result_declaration;
	int supported = prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			result_type,
			&result_type_id,
			&result_declaration
		) == 0 && result_declaration && hott_ordinary_adt_identity_supported(
			terms,
			type_declarations,
			contexts,
			result_type,
			result_declaration
		);
	(void)domain_type_id;
	(void)result_type_id;
	(void)parameter_context;
	return supported;
}

static int hott_type_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t pi
) {
	if (!terms || !type_declarations || pi >= terms->term_count ||
		terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	uint32_t codomain_binding;
	uint32_t codomain;
	int supported = prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) == 0 && codomain < terms->term_count &&
		terms->terms[codomain].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		!prototype_term_contains_free_binding(terms, codomain, codomain_binding);
	(void)type_declarations;
	(void)parameter_context;
	return supported;
}

static int hott_type_former_descriptor_finalize(
	struct prototype_hott_type_former_descriptor* descriptor
) {
	if (!descriptor || !descriptor->admitted) {
		return 0;
	}
	descriptor->capabilities =
		(struct prototype_hott_type_former_capabilities) {
			.relation_type_action = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.term_action = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.ordinary_reindex = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.purity = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.identity_computation = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.transport = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.lifting = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.resource_hook = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.artifact = PROTOTYPE_HOTT_CAPABILITY_DEFERRED
		};
	switch (descriptor->kind) {
	case PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_ADT_FIELD) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_PI:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_RELATED_INPUT) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_RELATION);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_PURE_COMPUTATION:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_RESULT_RELATION);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_THUNK:
		descriptor->child_role_mask = UINT64_C(1) <<
			PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_RELATION;
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_RELATION:
	case PROTOTYPE_HOTT_TYPE_FORMER_DIMENSION_ACTION:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_SUBSTITUTION_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_RELATION_TYPE_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_TERM_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY);
		break;
	default:
		return -1;
	}
	return descriptor->child_role_mask != 0 ? 0 : -1;
}

int prototype_hott_type_former_descriptor_query(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	struct prototype_hott_type_former_descriptor* p_descriptor
) {
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_claim_id);
	if (!terms || !type_declarations || !contexts || !dimension_operators ||
		!source || !p_descriptor ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->subject >= terms->term_count) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor = {
		.kind = PROTOTYPE_HOTT_TYPE_FORMER_UNKNOWN,
		.admitted = 0,
		.relation_type_action_rule = PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_NONE,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
		.source_claim_id = source_claim_id,
		.source_type_term_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject
	};
	uint32_t exposed_type = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
	struct prototype_term_normalization_result exposure;
	if (prototype_term_normalize_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
			64,
			&exposure
		) == 0 && exposure.status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE &&
		exposure.term_id < terms->term_count) {
		exposed_type = exposure.term_id;
	}
	const struct prototype_term* type = &terms->terms[exposed_type];
	uint32_t acted_family;
	uint32_t acted_source;
	uint32_t acted_operator;
	uint32_t acted_dimension;
	size_t acted_argument_count;
	int acted_instance = prototype_dimension_action_family_instance_info(
		terms,
		dimension_operators,
		exposed_type,
		&acted_family,
		&acted_source,
		&acted_operator,
		&acted_dimension,
		NULL,
		0,
		&acted_argument_count
	);
	uint32_t ordinary_type_id;
	const struct prototype_type_declaration* ordinary_type;
	if (acted_instance == 0) {
		descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_DIMENSION_ACTION;
		descriptor.admitted = 1;
		descriptor.relation_type_action_rule =
			PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_DIMENSION_HIGHER;
		descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	} else if (acted_instance < 0) {
		return -1;
	} else if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
		exposed_type,
			&ordinary_type_id,
			&ordinary_type
		) == 0 && ordinary_type_id < type_declarations->type_count &&
		ordinary_type) {
		descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT;
		descriptor.admitted = 1;
		descriptor.relation_type_action_rule = hott_zero_field_ordinary_adt(
			terms, type_declarations, contexts, exposed_type
		) ? PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ZERO_FIELD_ADT :
			PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ADT_TELESCOPE;
		descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	} else {
		uint32_t relation_left_type;
		uint32_t relation_right_type;
		uint32_t relation_left;
		uint32_t relation_right;
		switch (type->tag) {
		case PROTOTYPE_TERM_TYPE_VIEW:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ADT_TELESCOPE;
			break;
		case PROTOTYPE_TERM_PI:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_PI;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PI_POINTWISE;
			{
				int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				int status = hott_term_fragment_scan(
					terms, prototype_judgement_proposition_get(judgement, source->proposition_id)->subject, 0, &reason
				);
				if (status < 0) {
					return -1;
				}
				if (status == 0) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = reason;
				}
			}
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_PURE_COMPUTATION;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PURE_COMPUTATION;
			switch (prototype_term_effect_row_purity(
				terms, type->as.computation_type.label
			)) {
			case PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL:
				descriptor.residual_reason =
					PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL;
				break;
			case PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED:
				descriptor.residual_reason =
					PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
				break;
			default:
				descriptor.admitted = 1;
				descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				break;
			}
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_THUNK;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_THUNK;
			if (type->as.thunk_type.computation < terms->term_count &&
				terms->terms[type->as.thunk_type.computation].tag ==
					PROTOTYPE_TERM_COMPUTATION_TYPE) {
				int purity = prototype_term_effect_row_purity(
					terms,
					terms->terms[type->as.thunk_type.computation].
						as.computation_type.label
				);
				if (purity == PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = purity ==
						PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL ?
						PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL :
						PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
				}
			} else if (type->as.thunk_type.computation < terms->term_count &&
				terms->terms[type->as.thunk_type.computation].tag ==
					PROTOTYPE_TERM_PI) {
				int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				int status = hott_term_fragment_scan(
					terms, type->as.thunk_type.computation, 0, &reason
				);
				if (status < 0) {
					return -1;
				}
				if (status == 0) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = reason;
				}
			}
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE;
			descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_UNIVERSE;
			break;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_HOST_PRIMITIVE;
			descriptor.residual_reason =
				PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
			break;
		default:
			if (prototype_term_relation_type_info(
					terms,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
					&relation_left_type,
					&relation_right_type,
					&relation_left,
					&relation_right
				) == 0) {
				descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_RELATION;
				descriptor.admitted = 1;
				descriptor.relation_type_action_rule =
					PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_RELATION_HIGHER;
				descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
			}
			break;
		}
	}
	if (hott_type_former_descriptor_finalize(&descriptor) != 0) {
		return -1;
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT &&
		descriptor.admitted) {
		uint32_t source_type_id;
		const struct prototype_type_declaration* source_type;
		if (prototype_type_view_declaration_query(
				type_declarations,
				contexts,
				terms,
				exposed_type,
				&source_type_id,
				&source_type
		) == 0 && source_type_id < type_declarations->type_count &&
		hott_ordinary_adt_identity_supported(
				terms,
				type_declarations,
				contexts,
				exposed_type,
				source_type
			)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_DIMENSION_ACTION &&
		descriptor.admitted) {
		descriptor.capabilities.identity_computation =
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI &&
		descriptor.admitted) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t identity_context = source_context;
		if (source_context != prototype_context_empty(contexts) &&
			hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			)) {
			identity_context = prototype_context_empty(contexts);
		}
		if (hott_type_pi_identity_supported(
				terms,
				type_declarations,
				identity_context,
				exposed_type
			)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK &&
		descriptor.admitted) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t identity_context = source_context;
		if (source_context != prototype_context_empty(contexts) &&
			hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			)) {
			identity_context = prototype_context_empty(contexts);
		}
		if (hott_thunk_return_identity_supported(
			terms,
			type_declarations,
			identity_context,
			exposed_type
		) || hott_thunk_pi_identity_supported(
			terms,
			type_declarations,
			contexts,
			identity_context,
			exposed_type
		)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	}
	const struct prototype_judgement_proposition* source_proposition =
		prototype_judgement_proposition_get(judgement, source->proposition_id);
	if (source_proposition &&
		source_proposition->context_id == prototype_context_empty(contexts) &&
		descriptor.capabilities.identity_computation ==
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED &&
		(descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT ||
		 descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI ||
		 descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK)) {
		descriptor.capabilities.artifact = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
	}
	*p_descriptor = descriptor;
	return 0;
}
