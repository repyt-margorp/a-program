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

static int hott_generated_identity_declaration(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t source_carrier,
	uint32_t parameter_context,
	uint32_t* p_type_id
) {
	if (!type_declarations || !p_type_id) {
		return -1;
	}
	for (uint32_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (type->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			type->origin_source_carrier_term_id == source_carrier &&
			type->parameter_context == parameter_context &&
			type->formation_classifier != PROTOTYPE_INVALID_ID) {
			*p_type_id = i;
			return 0;
		}
	}
	return 1;
}

static int hott_adt_constructor_field_count(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t* p_field_count
) {
	if (!type_declarations || !contexts || !source_type || !p_field_count ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t field_path[64];
	return prototype_context_extension_path(
		contexts,
		constructor->parameter_context,
		constructor->field_context,
		field_path,
		64,
		p_field_count
	);
}

static int hott_indexed_constructor_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal
) {
	uint32_t field_count;
	if (!terms || !type_declarations || !contexts || !source_type ||
		hott_adt_constructor_field_count(
			type_declarations,
			contexts,
			source_type,
			constructor_ordinal,
			&field_count
		) != 0) {
		return -1;
	}
	if (source_type->index_count == 0) {
		return 1;
	}
	/* Non-nullary indexed constructors require lifting their result indices
	 * through the field telescope. That rule remains residual. */
	if (field_count != 0) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	/* This fragment selects one exact indexed fiber. Conversion is not enough:
	 * replay must recover the same constructor set without normalization work. */
	(void)terms;
	return constructor->result_classifier == source_carrier;
}

static int hott_generated_constructor_index_for_source_ordinal(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t generated_type_id,
	uint32_t source_carrier,
	uint32_t source_ordinal,
	uint32_t* p_generated_index
) {
	if (!terms || !type_declarations || !p_generated_index ||
		generated_type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* generated =
		&type_declarations->type_declarations[generated_type_id];
	for (uint32_t i = 0; i < generated->constructor_count; ++i) {
		uint32_t constructor_id = generated->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t result_type_id;
		uint32_t endpoints[2];
		uint32_t endpoint_count;
		uint32_t left_head;
		uint32_t left_owner;
		uint32_t left_ordinal;
		uint32_t left_arguments[64];
		uint32_t left_argument_count;
		uint32_t right_head;
		uint32_t right_owner;
		uint32_t right_ordinal;
		uint32_t right_arguments[64];
		uint32_t right_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				constructor->result_classifier,
				&result_type_id,
				endpoints,
				&endpoint_count
			) != 0 || result_type_id != generated_type_id || endpoint_count != 2 ||
			prototype_term_constructor_spine_info(
				terms, endpoints[0], &left_head, &left_owner, &left_ordinal,
				left_arguments, 64, &left_argument_count
			) != 0 || prototype_term_constructor_spine_info(
				terms, endpoints[1], &right_head, &right_owner, &right_ordinal,
				right_arguments, 64, &right_argument_count
			) != 0) {
			return -1;
		}
		if (left_owner == source_carrier && right_owner == source_carrier &&
			left_ordinal == source_ordinal && right_ordinal == source_ordinal) {
			*p_generated_index = constructor->constructor_index;
			return 0;
		}
		(void)left_head;
		(void)right_head;
		(void)left_arguments;
		(void)right_arguments;
		(void)left_argument_count;
		(void)right_argument_count;
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
			uint32_t ignored_identity;
			if (field_classifier != source_carrier &&
				hott_generated_identity_declaration(
					type_declarations,
					field_classifier,
					source_type->parameter_context,
					&ignored_identity
				) != 0) {
				return 0;
			}
		}
	}
	(void)source_arguments;
	return 1;
}

static int hott_constructor_has_recursive_field(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_constructor_declaration* constructor
) {
	uint32_t fields[64];
	uint32_t field_count;
	if (!terms || !contexts || !constructor || prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			fields,
			64,
			&field_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		const struct prototype_context* field = prototype_context_get(
			contexts, fields[i]
		);
		if (!field) {
			return -1;
		}
		if (prototype_context_classifier_term(field) == source_carrier) {
			return 1;
		}
	}
	return 0;
}

static int hott_indexed_higher_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_type_declaration* source_identity
) {
	if (!terms || !type_declarations || !contexts || !source_identity ||
		source_identity->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source_identity->parameter_count != 0 || source_identity->index_count != 2) {
		return 0;
	}
	uint32_t original_type_id;
	const struct prototype_type_declaration* original;
	uint32_t original_carrier = source_identity->origin_source_carrier_term_id;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			original_carrier,
			&original_type_id,
			&original
		) != 0 || original_type_id >= type_declarations->type_count ||
		!original || !hott_ordinary_adt_identity_supported(
			terms,
			type_declarations,
			contexts,
			original_carrier,
			original
		)) {
		return 0;
	}
	/* The current eight-face lift only builds products of independent field
	 * squares. A field whose classifier uses an earlier constructor binder
	 * needs a dependent square and recursive coherence data. Keep that case
	 * residual instead of exposing a partial higher identity. */
	for (uint32_t i = 0; i < original->constructor_count; ++i) {
		uint32_t constructor_id = original->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t fields[64];
		uint32_t field_count;
		if (prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				fields,
				64,
				&field_count
			) != 0) {
			return 0;
		}
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field = prototype_context_get(
				contexts, fields[j]
			);
			if (!field) {
				return 0;
			}
			uint32_t classifier = prototype_context_classifier_term(field);
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous = prototype_context_get(
					contexts, fields[k]
				);
				if (!previous || prototype_term_contains_free_binding(
						terms, classifier, previous->binding_id
					)) {
					return 0;
				}
			}
		}
	}
	return 1;
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
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
	if (computation >= terms->term_count ||
		terms->terms[computation].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_effect_row_purity(
			terms, terms->terms[computation].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t result_carrier =
		terms->terms[computation].as.computation_type.result;
	uint32_t result_identity;
	return hott_generated_identity_declaration(
		type_declarations,
		result_carrier,
		parameter_context,
		&result_identity
	) == 0 || hott_thunk_pi_identity_supported(
		terms, type_declarations, parameter_context, result_carrier
	);
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t thunk_type
) {
	if (!terms || !type_declarations || thunk_type >= terms->term_count ||
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
	uint32_t domain_identity;
	if (hott_generated_identity_declaration(
			type_declarations,
			terms->terms[pi].as.pi.domain,
			parameter_context,
			&domain_identity
		) != 0) {
		return 0;
	}
	for (uint32_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* identity =
			&type_declarations->type_declarations[i];
		uint32_t carrier = identity->origin_source_carrier_term_id;
		if (identity->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			carrier < terms->term_count &&
			terms->terms[carrier].tag == PROTOTYPE_TERM_THUNK_TYPE &&
			terms->terms[carrier].as.thunk_type.computation == codomain &&
			identity->formation_classifier != PROTOTYPE_INVALID_ID) {
			return 1;
		}
	}
	return 0;
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
	uint32_t domain_identity;
	uint32_t codomain_identity;
	return prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) == 0 && codomain < terms->term_count &&
		terms->terms[codomain].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		!prototype_term_contains_free_binding(terms, codomain, codomain_binding) &&
		hott_generated_identity_declaration(
			type_declarations,
			terms->terms[pi].as.pi.domain,
			parameter_context,
			&domain_identity
		) == 0 && hott_generated_identity_declaration(
			type_declarations,
			codomain,
			parameter_context,
			&codomain_identity
		) == 0;
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
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	struct prototype_hott_type_former_descriptor* p_descriptor
) {
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_claim_id);
	if (!terms || !type_declarations || !contexts || !source || !p_descriptor ||
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
	uint32_t ordinary_type_id;
	const struct prototype_type_declaration* ordinary_type;
	if (prototype_type_view_declaration_query(
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
		(source_type->origin_kind ==
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ?
			hott_indexed_higher_identity_supported(
				terms, type_declarations, contexts, source_type
			) : hott_ordinary_adt_identity_supported(
				terms,
				type_declarations,
				contexts,
				exposed_type,
				source_type
			))) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
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
			identity_context,
			exposed_type
		)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE &&
		prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->classifier < terms->term_count && terms->terms[
			prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->classifier
		].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t generated_identity;
		if (source_context == prototype_context_empty(contexts) ||
			(hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			) && prototype_type_declaration_find_generated_identity(
				type_declarations,
				exposed_type,
				prototype_context_empty(contexts),
				&generated_identity
			) == 0)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	}
	*p_descriptor = descriptor;
	return 0;
}

