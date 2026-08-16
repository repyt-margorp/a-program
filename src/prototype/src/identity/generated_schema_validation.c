#include "a_program/core/term.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/type_declaration.h"

static int type_declaration_present(
	const struct prototype_type_declaration* type
) {
	return type && type->type_index != PROTOTYPE_INVALID_ID;
}

static int term_is_binding_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t binding_id
);

static int term_app_spine_parts(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
) {
	if (!terms || !p_head || !arguments || !p_argument_count ||
		term_id >= terms->term_count) {
		return -1;
	}
	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = term_id;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16 || count >= argument_capacity) {
			return -1;
		}
		reversed[count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_head = current;
	*p_argument_count = count;
	return 0;
}

static int contextual_field_identity_classifier_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t identity_classifier,
	uint32_t left_classifier,
	uint32_t right_classifier,
	uint32_t left_endpoint,
	uint32_t right_endpoint
) {
	uint32_t head;
	uint32_t arguments[5];
	uint32_t argument_count;
	if (term_app_spine_parts(
			terms,
			identity_classifier,
			&head,
			arguments,
			5,
			&argument_count
		) != 0 || argument_count != 5 || head >= terms->term_count ||
		arguments[2] >= terms->term_count ||
		arguments[3] != left_endpoint || arguments[4] != right_endpoint) {
		return 0;
	}
	struct prototype_term_conversion_result left = { 0 };
	struct prototype_term_conversion_result right = { 0 };
	if (!db || prototype_term_compare_for_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			arguments[0],
			left_classifier,
			UINT64_MAX,
			&left
		) != 0 || left.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		prototype_term_compare_for_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			arguments[1],
			right_classifier,
			UINT64_MAX,
			&right
		) != 0 || right.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	/* The third argument is the object action on the dependent field type.
	 * Its formation Claim is validated by the HOTT bridge certificate; this
	 * structural validator checks that the constructor consumes that action
	 * at the matching fibers and endpoints. */
	return 1;
}

static int generated_adt_identity_constructor_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	const struct prototype_type_constructor_declaration* generated_constructor,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id
) {
	if (!terms || !db || !contexts || !source_type || !generated_constructor ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			db->constructor_count || generated_constructor->owner_type !=
			generated_type_id) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&db->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	if (source_constructor->owner_type != source_type->type_index ||
		source_constructor->constructor_index != constructor_ordinal ||
		generated_constructor->constructor_index >=
			db->type_declarations[generated_type_id].constructor_count) {
		return 0;
	}
	uint32_t source_fields[64];
	uint32_t generated_fields[192];
	uint32_t source_field_count;
	uint32_t generated_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0 || prototype_context_extension_path(
			contexts,
			generated_constructor->parameter_context,
			generated_constructor->field_context,
			generated_fields,
			192,
			&generated_field_count
		) != 0 || generated_field_count != source_field_count * 3) {
		return 0;
	}
	uint32_t result_type_id;
	uint32_t result_arguments[2];
	uint32_t result_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			generated_constructor->result_classifier,
			&result_type_id,
			result_arguments,
			&result_argument_count
		) != 0 || result_type_id != generated_type_id ||
		result_argument_count != 2) {
		return 0;
	}
	uint32_t left_head;
	uint32_t right_head;
	uint32_t left_owner;
	uint32_t right_owner;
	uint32_t left_ordinal;
	uint32_t right_ordinal;
	uint32_t left_arguments[64];
	uint32_t right_arguments[64];
	uint32_t left_argument_count;
	uint32_t right_argument_count;
	if (prototype_term_constructor_spine_info(
			terms, result_arguments[0], &left_head, &left_owner, &left_ordinal,
			left_arguments, 64, &left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms, result_arguments[1], &right_head, &right_owner, &right_ordinal,
			right_arguments, 64, &right_argument_count
		) != 0 || left_owner != source_carrier || right_owner != source_carrier ||
		left_ordinal != constructor_ordinal || right_ordinal != constructor_ordinal ||
		left_argument_count != source_field_count ||
		right_argument_count != source_field_count) {
		return 0;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* source_field =
			prototype_context_get(contexts, source_fields[i]);
		const struct prototype_context* left_field =
			prototype_context_get(contexts, generated_fields[i * 3]);
		const struct prototype_context* right_field =
			prototype_context_get(contexts, generated_fields[i * 3 + 1]);
		const struct prototype_context* identity_field =
			prototype_context_get(contexts, generated_fields[i * 3 + 2]);
		uint32_t field_identity_type_id = generated_type_id;
		uint32_t identity_type_id;
		uint32_t identity_arguments[2];
		uint32_t identity_argument_count;
		if (!source_field || !left_field || !right_field || !identity_field ||
			!term_is_binding_var(
				terms, left_arguments[i], left_field->binding_id
			) || !term_is_binding_var(
				terms, right_arguments[i], right_field->binding_id
			)) {
			return 0;
		}
		uint32_t field_classifier = prototype_context_classifier_term(source_field);
		uint32_t left_classifier = prototype_context_classifier_term(left_field);
		uint32_t right_classifier = prototype_context_classifier_term(right_field);
		uint32_t source_binders[64];
		uint32_t left_binders[64];
		uint32_t right_binders[64];
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_context* previous_source =
				prototype_context_get(contexts, source_fields[j]);
			const struct prototype_context* previous_left =
				prototype_context_get(contexts, generated_fields[j * 3]);
			const struct prototype_context* previous_right =
				prototype_context_get(contexts, generated_fields[j * 3 + 1]);
			if (!previous_source || !previous_left || !previous_right) {
				return 0;
			}
			source_binders[j] = previous_source->binding_id;
			left_binders[j] = previous_left->binding_id;
			right_binders[j] = previous_right->binding_id;
		}
		int left_equal;
		int right_equal;
		if (prototype_term_core_shape_equal_under_binders(
				terms,
				source_binders,
				left_binders,
				i,
				field_classifier,
				left_classifier,
				&left_equal
			) != 0 || prototype_term_core_shape_equal_under_binders(
				terms,
				source_binders,
				right_binders,
				i,
				field_classifier,
				right_classifier,
				&right_equal
			) != 0 || !left_equal || !right_equal) {
			return 0;
		}
		int dependent = 0;
		for (uint32_t j = 0; j < i; ++j) {
			if (prototype_term_contains_free_binding(
					terms, field_classifier, source_binders[j]
				)) {
				dependent = 1;
				break;
			}
		}
		if (!dependent && field_classifier != source_carrier &&
			prototype_type_declaration_find_generated_identity(
				db,
				field_classifier,
				db->type_declarations[generated_type_id].parameter_context,
				&field_identity_type_id
			) != 0) {
			return 0;
		}
		if (dependent) {
			if (!contextual_field_identity_classifier_valid(
					terms,
					db,
					prototype_context_classifier_term(identity_field),
					left_classifier,
					right_classifier,
					left_arguments[i],
					right_arguments[i]
				)) {
				return 0;
			}
		} else if (prototype_term_type_instance_info(
				terms,
				prototype_context_classifier_term(identity_field),
				&identity_type_id,
				identity_arguments,
				&identity_argument_count
			) != 0 || identity_type_id != field_identity_type_id ||
			identity_argument_count != 2 ||
			identity_arguments[0] != left_arguments[i] ||
			identity_arguments[1] != right_arguments[i]) {
			return 0;
		}
	}
	(void)left_head;
	(void)right_head;
	return 1;
}

static int generated_identity_declaration_header_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id
) {
	if (!terms || !db || !contexts || source_carrier >= terms->term_count ||
		generated_type_id >= db->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* generated =
		&db->type_declarations[generated_type_id];
	uint32_t index_path[2];
	uint32_t index_count;
	uint32_t outer_binding;
	uint32_t outer_body;
	uint32_t inner_binding;
	uint32_t inner_body;
	if (!type_declaration_present(generated) || generated->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		generated->origin_source_carrier_term_id != source_carrier ||
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
		) != source_carrier || prototype_context_classifier_term(
			prototype_context_get(contexts, index_path[1])
		) != source_carrier || generated->formation_classifier >=
			terms->term_count || terms->terms[
			generated->formation_classifier
		].tag != PROTOTYPE_TERM_PI || terms->terms[
			generated->formation_classifier
		].as.pi.domain != source_carrier || prototype_term_pure_family_parts(
			terms,
			terms->terms[generated->formation_classifier].as.pi.codomain_family,
			&outer_binding,
			&outer_body
		) != 0 || outer_body >= terms->term_count ||
		terms->terms[outer_body].tag != PROTOTYPE_TERM_PI ||
		terms->terms[outer_body].as.pi.domain != source_carrier ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[outer_body].as.pi.codomain_family,
			&inner_binding,
			&inner_body
		) != 0 || inner_body >= terms->term_count ||
		terms->terms[inner_body].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 0;
	}
	(void)outer_binding;
	(void)inner_binding;
	return 1;
}

static int term_is_binding_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t binding_id
) {
	return terms && term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[term_id].as.var.binding_id == binding_id;
}

int prototype_type_declaration_generated_identity_rule_for_source(
	const struct prototype_term_db* terms,
	uint32_t source_carrier
) {
	if (!terms || source_carrier >= terms->term_count) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	if (terms->terms[source_carrier].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT;
	}
	if (terms->terms[source_carrier].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE;
	}
	if (terms->terms[source_carrier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	uint32_t computation = terms->terms[source_carrier].as.thunk_type.computation;
	if (computation >= terms->term_count ||
		terms->terms[computation].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_effect_row_purity(
			terms, terms->terms[computation].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN;
}

static int generated_identity_constructor_source_ordinal(
	const struct prototype_term_db* terms,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	const struct prototype_type_constructor_declaration* constructor,
	uint32_t* p_source_ordinal
) {
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
	if (!terms || !constructor || !p_source_ordinal ||
		prototype_term_type_instance_info(
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
		) != 0 || left_owner != source_carrier ||
		right_owner != source_carrier || left_ordinal != right_ordinal ||
		left_argument_count != right_argument_count) {
		return -1;
	}
	*p_source_ordinal = left_ordinal;
	(void)left_head;
	(void)right_head;
	(void)left_arguments;
	(void)right_arguments;
	return 0;
}

static int indexed_nullary_source_constructor_compatible(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t source_ordinal
) {
	if (!terms || !db || !contexts || !source_type ||
		source_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + source_ordinal >= db->constructor_count) {
		return -1;
	}
	if (source_type->index_count == 0) {
		return 1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&db->constructor_declarations[
			source_type->first_constructor + source_ordinal
		];
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
		return -1;
	}
	return field_count == 0 && constructor->result_classifier == source_carrier;
}

static int validate_generated_identity_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t remaining_depth
);

static int generated_identity_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi,
	uint32_t* p_domain,
	uint32_t* p_binding,
	uint32_t* p_codomain
) {
	if (!terms || !p_domain || !p_binding || !p_codomain ||
		pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			p_binding,
			p_codomain
		) != 0) {
		return 0;
	}
	*p_domain = terms->terms[pi].as.pi.domain;
	return 1;
}

static int validate_universe_correspondence_identity(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t universe,
	uint32_t generated_type_id
) {
	const struct prototype_type_declaration* generated =
		generated_type_id < db->type_count ?
		&db->type_declarations[generated_type_id] : NULL;
	uint32_t fields[7];
	uint32_t field_count;
	if (!terms || !db || !contexts || !generated ||
		universe >= terms->term_count ||
		terms->terms[universe].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		generated->constructor_count != 1 ||
		generated->first_constructor >= db->constructor_count) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&db->constructor_declarations[generated->first_constructor];
	if (constructor->owner_type != generated_type_id ||
		constructor->constructor_index != 0 ||
		constructor->parameter_context != generated->parameter_context ||
		prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			fields,
			7,
			&field_count
		) != 0 || field_count != 7) {
		return 0;
	}
	const struct prototype_context* field[7];
	for (uint32_t i = 0; i < 7; ++i) {
		field[i] = prototype_context_get(contexts, fields[i]);
		if (!field[i]) {
			return 0;
		}
	}
	if (prototype_context_classifier_term(field[0]) != universe ||
		prototype_context_classifier_term(field[1]) != universe) {
		return 0;
	}

	uint32_t domain;
	uint32_t binding;
	uint32_t codomain;
	uint32_t inner_domain;
	uint32_t inner_binding;
	uint32_t inner_codomain;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[2]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		!generated_identity_pi_parts(
			terms,
			codomain,
			&inner_domain,
			&inner_binding,
			&inner_codomain
		) || !term_is_binding_var(
			terms, inner_domain, field[1]->binding_id
		) || inner_codomain != universe) {
		return 0;
	}
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[3]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		!term_is_binding_var(terms, codomain, field[1]->binding_id) ||
		!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[4]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[1]->binding_id) ||
		!term_is_binding_var(terms, codomain, field[0]->binding_id)) {
		return 0;
	}

	uint32_t right_lift_binding;
	uint32_t right_lift_type;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[5]),
			&domain,
			&right_lift_binding,
			&right_lift_type
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		right_lift_type >= terms->term_count ||
		terms->terms[right_lift_type].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	uint32_t relation_x = terms->terms[right_lift_type].as.app.function;
	uint32_t trr_x = terms->terms[right_lift_type].as.app.argument;
	if (relation_x >= terms->term_count || trr_x >= terms->term_count ||
		terms->terms[relation_x].tag != PROTOTYPE_TERM_APP ||
		terms->terms[trr_x].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(
			terms, terms->terms[relation_x].as.app.function,
			field[2]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[relation_x].as.app.argument,
			right_lift_binding
		) || !term_is_binding_var(
			terms, terms->terms[trr_x].as.app.function,
			field[3]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[trr_x].as.app.argument,
			right_lift_binding
		)) {
		return 0;
	}

	uint32_t left_lift_binding;
	uint32_t left_lift_type;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[6]),
			&domain,
			&left_lift_binding,
			&left_lift_type
		) || !term_is_binding_var(terms, domain, field[1]->binding_id) ||
		left_lift_type >= terms->term_count ||
		terms->terms[left_lift_type].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	uint32_t relation_left = terms->terms[left_lift_type].as.app.function;
	uint32_t y = terms->terms[left_lift_type].as.app.argument;
	if (relation_left >= terms->term_count || y >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(terms, y, left_lift_binding)) {
		return 0;
	}
	uint32_t relation = terms->terms[relation_left].as.app.function;
	uint32_t trl_y = terms->terms[relation_left].as.app.argument;
	if (relation >= terms->term_count || trl_y >= terms->term_count ||
		terms->terms[relation].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[trl_y].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(terms, relation, field[2]->binding_id) ||
		!term_is_binding_var(
			terms, terms->terms[trl_y].as.app.function, field[4]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[trl_y].as.app.argument, left_lift_binding
		)) {
		return 0;
	}

	uint32_t result_type_id;
	uint32_t result_arguments[2];
	uint32_t result_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			constructor->result_classifier,
			&result_type_id,
			result_arguments,
			&result_argument_count
		) != 0 || result_type_id != generated_type_id ||
		result_argument_count != 2 || !term_is_binding_var(
			terms, result_arguments[0], field[0]->binding_id
		) || !term_is_binding_var(
			terms, result_arguments[1], field[1]->binding_id
		)) {
		return 0;
	}
	(void)binding;
	(void)inner_binding;
	return 1;
}

static int validate_nondependent_pi_identity_family(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t left_endpoint_binding,
	uint32_t right_endpoint_binding,
	uint32_t identity_family,
	uint32_t remaining_depth
) {
	if (!terms || !db || !contexts || remaining_depth == 0 ||
		source_carrier >= terms->term_count || identity_family >=
			terms->term_count || terms->terms[source_carrier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t source_pi = terms->terms[source_carrier].as.thunk_type.computation;
	uint32_t source_family_binding;
	uint32_t source_codomain;
	if (source_pi >= terms->term_count || terms->terms[source_pi].tag !=
			PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[source_pi].as.pi.codomain_family,
			&source_family_binding,
			&source_codomain
		) != 0 || source_codomain >= terms->term_count ||
		terms->terms[source_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_contains_free_binding(
			terms, source_codomain, source_family_binding
		) || prototype_term_effect_row_purity(
			terms, terms->terms[source_codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t domain = terms->terms[source_pi].as.pi.domain;
	uint32_t codomain_thunk = PROTOTYPE_INVALID_ID;
	uint32_t domain_identity_type_id;
	uint32_t codomain_identity_type_id;
	for (uint32_t i = 0; i < terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_THUNK_TYPE &&
			terms->terms[i].as.thunk_type.computation == source_codomain) {
			codomain_thunk = i;
			break;
		}
	}
	if (codomain_thunk == PROTOTYPE_INVALID_ID ||
		prototype_type_declaration_find_generated_identity(
			db,
			domain,
			prototype_context_empty(contexts),
			&domain_identity_type_id
		) != 0 || prototype_type_declaration_find_generated_identity(
			db,
			codomain_thunk,
			prototype_context_empty(contexts),
			&codomain_identity_type_id
		) != 0 || !validate_generated_identity_depth(
			terms,
			db,
			contexts,
			domain,
			domain_identity_type_id,
			prototype_type_declaration_generated_identity_rule_for_source(
				terms, domain
			),
			remaining_depth - 1
		) || !validate_generated_identity_depth(
			terms,
			db,
			contexts,
			codomain_thunk,
			codomain_identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			remaining_depth - 1
		)) {
		return 0;
	}
	uint32_t current = identity_family;
	uint32_t binders[3];
	uint32_t bodies[3];
	uint32_t pi_terms[3];
	for (uint32_t i = 0; i < 3; ++i) {
		if (current >= terms->term_count || terms->terms[current].tag !=
				PROTOTYPE_TERM_THUNK_TYPE) {
			return 0;
		}
		pi_terms[i] = terms->terms[current].as.thunk_type.computation;
		if (pi_terms[i] >= terms->term_count || terms->terms[pi_terms[i]].tag !=
				PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
				terms,
				terms->terms[pi_terms[i]].as.pi.codomain_family,
				&binders[i],
				&bodies[i]
			) != 0 || bodies[i] >= terms->term_count ||
			terms->terms[bodies[i]].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			prototype_term_effect_row_purity(
				terms, terms->terms[bodies[i]].as.computation_type.label
			) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
			return 0;
		}
		if (i < 2) {
			current = terms->terms[bodies[i]].as.computation_type.result;
		}
	}
	if (terms->terms[pi_terms[0]].as.pi.domain != domain ||
		terms->terms[pi_terms[1]].as.pi.domain != domain) {
		return 0;
	}
	uint32_t domain_identity_type;
	uint32_t domain_identity_arguments[2];
	uint32_t domain_identity_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			terms->terms[pi_terms[2]].as.pi.domain,
			&domain_identity_type,
			domain_identity_arguments,
			&domain_identity_argument_count
		) != 0 || domain_identity_type != domain_identity_type_id ||
		domain_identity_argument_count != 2 || !term_is_binding_var(
			terms, domain_identity_arguments[0], binders[0]
		) || !term_is_binding_var(
			terms, domain_identity_arguments[1], binders[1]
		)) {
		return 0;
	}
	uint32_t result_identity = terms->terms[bodies[2]].as.computation_type.result;
	uint32_t result_identity_type;
	uint32_t result_identity_arguments[2];
	uint32_t result_identity_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			result_identity,
			&result_identity_type,
			result_identity_arguments,
			&result_identity_argument_count
		) != 0 || result_identity_type != codomain_identity_type_id ||
		result_identity_argument_count != 2) {
		return 0;
	}
	uint32_t expected_endpoint_bindings[2] = {
		left_endpoint_binding, right_endpoint_binding
	};
	uint32_t expected_input_bindings[2] = { binders[0], binders[1] };
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t thunk = result_identity_arguments[i];
		uint32_t application = thunk < terms->term_count &&
			terms->terms[thunk].tag == PROTOTYPE_TERM_THUNK ?
			terms->terms[thunk].as.thunk.computation : PROTOTYPE_INVALID_ID;
		uint32_t forced = application < terms->term_count &&
			terms->terms[application].tag == PROTOTYPE_TERM_APP ?
			terms->terms[application].as.app.function : PROTOTYPE_INVALID_ID;
		uint32_t argument = application < terms->term_count &&
			terms->terms[application].tag == PROTOTYPE_TERM_APP ?
			terms->terms[application].as.app.argument : PROTOTYPE_INVALID_ID;
		uint32_t endpoint = forced < terms->term_count &&
			terms->terms[forced].tag == PROTOTYPE_TERM_FORCE ?
			terms->terms[forced].as.force.value : PROTOTYPE_INVALID_ID;
		if (!term_is_binding_var(
				terms, endpoint, expected_endpoint_bindings[i]
			) || !term_is_binding_var(
				terms, argument, expected_input_bindings[i]
			)) {
			return 0;
		}
	}
	return binders[0] != binders[1] && binders[0] != binders[2] &&
		binders[1] != binders[2];
}

static int validate_generated_identity_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t remaining_depth
) {
	if (!terms || !db || !contexts || source_carrier >= terms->term_count ||
		generated_type_id >= db->type_count || remaining_depth == 0 ||
		!generated_identity_declaration_header_valid(
			terms, db, contexts, source_carrier, generated_type_id
		)) {
		return 0;
	}
	const struct prototype_type_declaration* generated =
		&db->type_declarations[generated_type_id];
	if (computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		return validate_universe_correspondence_identity(
			terms, db, contexts, source_carrier, generated_type_id
		);
	}
	if (computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
		uint32_t computation = terms->terms[source_carrier].tag ==
			PROTOTYPE_TERM_THUNK_TYPE ?
			terms->terms[source_carrier].as.thunk_type.computation :
			PROTOTYPE_INVALID_ID;
		uint32_t result_carrier = computation < terms->term_count &&
			terms->terms[computation].tag == PROTOTYPE_TERM_COMPUTATION_TYPE ?
			terms->terms[computation].as.computation_type.result :
			PROTOTYPE_INVALID_ID;
		uint32_t result_identity_type_id = PROTOTYPE_INVALID_ID;
		int result_has_generated_identity = result_carrier != PROTOTYPE_INVALID_ID &&
			prototype_type_declaration_find_generated_identity(
				db,
				result_carrier,
				generated->parameter_context,
				&result_identity_type_id
			) == 0;
		if (result_carrier == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_purity(
				terms, terms->terms[computation].as.computation_type.label
			) != PROTOTYPE_EFFECT_ROW_PURITY_PURE || generated->constructor_count != 1 ||
			(result_has_generated_identity && !validate_generated_identity_depth(
				terms,
				db,
				contexts,
				result_carrier,
				result_identity_type_id,
				prototype_type_declaration_generated_identity_rule_for_source(
					terms, result_carrier
				),
				remaining_depth - 1
			))) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&db->constructor_declarations[generated->first_constructor];
		uint32_t fields[3];
		uint32_t field_count;
		if (generated->first_constructor >= db->constructor_count ||
			constructor->owner_type != generated_type_id ||
			constructor->constructor_index != 0 ||
			constructor->parameter_context != generated->parameter_context ||
			prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				fields,
				3,
				&field_count
			) != 0 || field_count != 3) {
			return 0;
		}
		const struct prototype_context* left = prototype_context_get(
			contexts, fields[0]
		);
		const struct prototype_context* right = prototype_context_get(
			contexts, fields[1]
		);
		const struct prototype_context* identity = prototype_context_get(
			contexts, fields[2]
		);
		uint32_t identity_type_id;
		uint32_t identity_arguments[2];
		uint32_t identity_argument_count;
		uint32_t result_type_id;
		uint32_t result_arguments[2];
		uint32_t result_argument_count;
		if (!left || !right || !identity ||
			prototype_context_classifier_term(left) != result_carrier ||
			prototype_context_classifier_term(right) != result_carrier ||
			(result_has_generated_identity && (prototype_term_type_instance_info(
				terms,
				prototype_context_classifier_term(identity),
				&identity_type_id,
				identity_arguments,
				&identity_argument_count
			) != 0 || identity_type_id != result_identity_type_id ||
			identity_argument_count != 2 || !term_is_binding_var(
				terms, identity_arguments[0], left->binding_id
			) || !term_is_binding_var(
				terms, identity_arguments[1], right->binding_id
			))) || (!result_has_generated_identity &&
			!validate_nondependent_pi_identity_family(
				terms,
				db,
				contexts,
				result_carrier,
				left->binding_id,
				right->binding_id,
				prototype_context_classifier_term(identity),
				remaining_depth - 1
			)) || prototype_term_type_instance_info(
				terms,
				constructor->result_classifier,
				&result_type_id,
				result_arguments,
				&result_argument_count
			) != 0 || result_type_id != generated_type_id ||
			result_argument_count != 2) {
			return 0;
		}
		for (uint32_t i = 0; i < 2; ++i) {
			uint32_t thunk = result_arguments[i];
			uint32_t returned = thunk < terms->term_count &&
				terms->terms[thunk].tag == PROTOTYPE_TERM_THUNK ?
				terms->terms[thunk].as.thunk.computation : PROTOTYPE_INVALID_ID;
			uint32_t value = returned < terms->term_count &&
				terms->terms[returned].tag == PROTOTYPE_TERM_RETURN ?
				terms->terms[returned].as.return_term.value : PROTOTYPE_INVALID_ID;
			uint32_t expected_binding = i == 0 ?
				left->binding_id : right->binding_id;
			if (!term_is_binding_var(terms, value, expected_binding)) {
				return 0;
			}
		}
		return 1;
	}
	if (computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		terms->terms[source_carrier].tag != PROTOTYPE_TERM_TYPE_VIEW) {
		return 0;
	}
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_type;
	if (prototype_type_view_declaration_query(
			db, contexts, terms, source_carrier, &source_type_id, &source_type
		) != 0 || source_type_id >= db->type_count || !source_type ||
		source_type->parameter_count != 0 || source_type->constructor_count == 0) {
		return 0;
	}
	uint32_t source_arguments[16];
	uint32_t source_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			source_carrier,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_argument_count != source_type->index_count) {
		return 0;
	}
	uint32_t expected_constructor_count = 0;
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		int compatible = indexed_nullary_source_constructor_compatible(
			terms, db, contexts, source_carrier, source_type, i
		);
		if (compatible < 0) {
			return 0;
		}
		expected_constructor_count += compatible != 0;
	}
	if (generated->constructor_count != expected_constructor_count) {
		return 0;
	}
	for (uint32_t i = 0; i < generated->constructor_count; ++i) {
		uint32_t constructor_id = generated->first_constructor + i;
		uint32_t source_ordinal;
		if (constructor_id >= db->constructor_count ||
			generated_identity_constructor_source_ordinal(
				terms,
				source_carrier,
				generated_type_id,
				&db->constructor_declarations[constructor_id],
				&source_ordinal
			) != 0 || source_ordinal >= source_type->constructor_count ||
			indexed_nullary_source_constructor_compatible(
				terms,
				db,
				contexts,
				source_carrier,
				source_type,
				source_ordinal
			) != 1 ||
			!generated_adt_identity_constructor_valid(
				terms,
				db,
				contexts,
				source_carrier,
				source_type,
				&db->constructor_declarations[constructor_id],
				source_ordinal,
				generated_type_id
			)) {
			return 0;
		}
		for (uint32_t j = 0; j < i; ++j) {
			uint32_t previous_id = generated->first_constructor + j;
			uint32_t previous_ordinal;
			if (previous_id >= db->constructor_count ||
				generated_identity_constructor_source_ordinal(
					terms,
					source_carrier,
					generated_type_id,
					&db->constructor_declarations[previous_id],
					&previous_ordinal
				) != 0 || previous_ordinal == source_ordinal) {
				return 0;
			}
		}
	}
	(void)source_arguments;
	return 1;
}

int prototype_type_declaration_validate_generated_identity(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule
) {
	return validate_generated_identity_depth(
		terms,
		db,
		contexts,
		source_carrier,
		generated_type_id,
		computation_rule,
		terms ? (uint32_t)terms->term_count + 1 : 0
	);
}
