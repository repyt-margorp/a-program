static int hott_publish_action_residual(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	int reason,
	uint32_t* p_result_id
) {
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = PROTOTYPE_INVALID_ID,
		.outcome = {
			.state = reason == PROTOTYPE_HOTT_RESIDUAL_UNIVERSE ||
				reason == PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ?
				PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED :
				PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
			.residual_reason = reason,
			.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			.step_limit = 0,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	return prototype_hott_action_result_publish(actions, result, p_result_id);
}

static int hott_publish_ready_action_result(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	uint32_t certificate_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	if (!actions || !terms || !p_result_id || certificate_id >=
		actions->certificate_count || certificate_id + 1 !=
		actions->certificate_count || actions->certificates[certificate_id].request_id !=
		request_id) {
		return -1;
	}
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = certificate_id,
		.outcome = {
			.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	if (prototype_hott_action_result_publish(actions, result, p_result_id) != 0) {
		memset(&actions->certificates[certificate_id], 0,
			sizeof(actions->certificates[certificate_id]));
		actions->certificate_count--;
		return -1;
	}
	return 0;
}

static int hott_initialize_generated_identity_declaration(
	struct prototype_term_db* terms,
	struct prototype_context_db* contexts,
	uint32_t source_context,
	uint32_t source_carrier,
	uint32_t universe,
	struct prototype_type_declaration* generated
) {
	if (!terms || !contexts || !generated ||
		source_carrier >= terms->term_count || universe >= terms->term_count) {
		return -1;
	}
	if (generated->formation_classifier != PROTOTYPE_INVALID_ID) {
		return generated->parameter_context == source_context &&
			generated->parameter_count == 0 && generated->index_count == 2 ?
			0 : -1;
	}
	if (generated->parameter_context != source_context ||
		generated->index_context != PROTOTYPE_INVALID_ID ||
		generated->parameter_count != 0 || generated->index_count != 0 ||
		generated->constructor_count != 0) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t right_context;
	uint32_t right_family;
	uint32_t right_pi;
	uint32_t left_family;
	uint32_t formation_classifier;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			source_context,
			left_binding,
			source_carrier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			source_carrier,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_term_pure_family(
			terms, right_binding, universe, &right_family
		) != 0 || prototype_term_pi_family(
			terms, source_carrier, right_family, &right_pi
		) != 0 || prototype_term_pure_family(
			terms, left_binding, right_pi, &left_family
		) != 0 || prototype_term_pi_family(
			terms, source_carrier, left_family, &formation_classifier
		) != 0) {
		return -1;
	}
	generated->parameter_context = source_context;
	generated->index_context = right_context;
	generated->index_count = 2;
	generated->formation_classifier = formation_classifier;
	return 0;
}

static int hott_make_dependent_pi(
	struct prototype_term_db* terms,
	uint32_t domain,
	uint32_t binding_id,
	uint32_t codomain,
	uint32_t* p_pi
) {
	uint32_t family;
	if (!terms || !p_pi || binding_id == PROTOTYPE_INVALID_ID ||
		domain >= terms->term_count || codomain >= terms->term_count ||
		prototype_term_pure_family(
			terms, binding_id, codomain, &family
		) != 0) {
		return -1;
	}
	return prototype_term_pi_family(terms, domain, family, p_pi);
}

/* Universe identity is a selected correspondence with explicit two-sided
 * transport and fiber witnesses. These are pure type-level operations; they
 * are not the compiler-local RELATION_TYPE action and do not carry effects. */
static int hott_add_universe_correspondence_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	struct prototype_context_db* contexts,
	uint32_t universe,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	if (!terms || !types || !contexts || !generated || !p_constructor_id ||
		universe >= terms->term_count || generated->constructor_count != 0 ||
		generated->parameter_context != prototype_context_empty(contexts)) {
		return -1;
	}

	uint32_t bindings[7];
	uint32_t variables[7];
	uint32_t classifiers[7];
	uint32_t field_context = generated->parameter_context;
	for (uint32_t i = 0; i < 7; ++i) {
		bindings[i] = prototype_term_new_binding(terms);
		if (bindings[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	classifiers[0] = universe;
	classifiers[1] = universe;
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t next_context;
		if (prototype_context_extend(
				contexts,
				field_context,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		field_context = next_context;
	}

	uint32_t relation_right_binding = prototype_term_new_binding(terms);
	uint32_t relation_right_pi;
	if (relation_right_binding == PROTOTYPE_INVALID_ID ||
		hott_make_dependent_pi(
			terms,
			variables[1],
			relation_right_binding,
			universe,
			&relation_right_pi
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[0],
			prototype_term_new_binding(terms),
			relation_right_pi,
			&classifiers[2]
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[0],
			prototype_term_new_binding(terms),
			variables[1],
			&classifiers[3]
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[1],
			prototype_term_new_binding(terms),
			variables[0],
			&classifiers[4]
		) != 0) {
		return -1;
	}
	for (uint32_t i = 2; i < 5; ++i) {
		uint32_t next_context;
		if (prototype_context_extend(
				contexts,
				field_context,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		field_context = next_context;
	}

	uint32_t x_binding = prototype_term_new_binding(terms);
	uint32_t x;
	uint32_t trr_x;
	uint32_t relation_x;
	uint32_t right_lift;
	if (x_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, x_binding, &x
		) != 0 || prototype_term_app(
			terms, variables[3], x, &trr_x
		) != 0 || prototype_term_app(
			terms, variables[2], x, &relation_x
		) != 0 || prototype_term_app(
			terms, relation_x, trr_x, &right_lift
		) != 0 || hott_make_dependent_pi(
			terms, variables[0], x_binding, right_lift, &classifiers[5]
		) != 0) {
		return -1;
	}
	uint32_t next_context;
	if (prototype_context_extend(
			contexts,
			field_context,
			bindings[5],
			classifiers[5],
			PROTOTYPE_INVALID_ID,
			&next_context
		) != 0 || prototype_term_var(
			terms, bindings[5], &variables[5]
		) != 0) {
		return -1;
	}
	field_context = next_context;

	uint32_t y_binding = prototype_term_new_binding(terms);
	uint32_t y;
	uint32_t trl_y;
	uint32_t relation_left;
	uint32_t left_lift;
	if (y_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, y_binding, &y
		) != 0 || prototype_term_app(
			terms, variables[4], y, &trl_y
		) != 0 || prototype_term_app(
			terms, variables[2], trl_y, &relation_left
		) != 0 || prototype_term_app(
			terms, relation_left, y, &left_lift
		) != 0 || hott_make_dependent_pi(
			terms, variables[1], y_binding, left_lift, &classifiers[6]
		) != 0 || prototype_context_extend(
			contexts,
			field_context,
			bindings[6],
			classifiers[6],
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0 || prototype_term_var(
			terms, bindings[6], &variables[6]
		) != 0) {
		return -1;
	}

	uint32_t result_arguments[2] = { variables[0], variables[1] };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			field_context,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[7];
	for (uint32_t i = 0; i < 7; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		types,
		generated_type_id,
		-1,
		readback_fields,
		7,
		PROTOTYPE_INVALID_ID,
		generated->parameter_context,
		field_context,
		result_classifier,
		curried_classifier,
		p_constructor_id
	);
}

static int hott_initialize_indexed_higher_identity_declaration(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	struct prototype_context_db* contexts,
	uint32_t source_type_id,
	uint32_t generated_type_id,
	uint32_t universe
) {
	if (!terms || !types || !contexts || source_type_id >= types->type_count ||
		generated_type_id >= types->type_count || universe >= terms->term_count) {
		return -1;
	}
	const struct prototype_type_declaration* source =
		&types->type_declarations[source_type_id];
	struct prototype_type_declaration* generated =
		&types->type_declarations[generated_type_id];
	uint32_t empty = prototype_context_empty(contexts);
	if (generated->formation_classifier != PROTOTYPE_INVALID_ID) {
		return generated->parameter_context == empty &&
			generated->index_count == 8 ? 0 : -1;
	}
	if (source->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source->parameter_context != empty || source->index_count != 2 ||
		source->constructor_count == 0 || generated->parameter_context != empty ||
		generated->index_context != PROTOTYPE_INVALID_ID ||
		generated->index_count != 0 || generated->constructor_count != 0) {
		return -1;
	}
	uint32_t carrier = source->origin_source_carrier_term_id;
	uint32_t bindings[8];
	uint32_t variables[8];
	uint32_t index_contexts[8];
	uint32_t current = empty;
	for (uint32_t i = 0; i < 8; ++i) {
		bindings[i] = prototype_term_new_binding(terms);
		if (bindings[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	uint32_t classifiers[8];
	classifiers[0] = carrier;
	classifiers[1] = carrier;
	for (uint32_t i = 0; i < 8; ++i) {
		if (i == 2 || i == 5 || i == 6 || i == 7) {
			uint32_t left_index = i == 2 ? 0 : i == 5 ? 3 : i == 6 ? 0 : 1;
			uint32_t right_index = i == 2 ? 1 : i == 5 ? 4 : i == 6 ? 3 : 4;
			uint32_t arguments[2] = {
				variables[left_index], variables[right_index]
			};
			if (prototype_term_type_instance_make(
					terms, types, source_type_id, arguments, 2, &classifiers[i]
				) != 0) {
				return -1;
			}
		} else if (i == 3 || i == 4) {
			classifiers[i] = carrier;
		}
		if (prototype_context_extend(
				contexts,
				current,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&index_contexts[i]
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		current = index_contexts[i];
	}
	uint32_t formation_classifier;
	if (prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			empty,
			current,
			universe,
			&formation_classifier
		) != 0) {
		return -1;
	}
	generated->index_context = current;
	generated->index_count = 8;
	generated->formation_classifier = formation_classifier;
	uint32_t source_carrier = source->origin_source_carrier_term_id;
	uint32_t original_type_id;
	const struct prototype_type_declaration* original;
	if (prototype_type_view_declaration_query(
			types,
			contexts,
			terms,
			source_carrier,
			&original_type_id,
			&original
		) != 0 || !original || original->index_count != 0 ||
		original->parameter_count != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < original->constructor_count; ++i) {
		uint32_t original_constructor_id = original->first_constructor + i;
		if (original_constructor_id >= types->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* original_constructor =
			&types->constructor_declarations[original_constructor_id];
		uint32_t original_fields[64];
		uint32_t original_field_count;
		if (prototype_context_extension_path(
				contexts,
				original_constructor->parameter_context,
				original_constructor->field_context,
				original_fields,
				64,
				&original_field_count
			) != 0) {
			return -1;
		}
		uint32_t lower_constructor_index;
		if (hott_generated_constructor_index_for_source_ordinal(
				terms,
				types,
				source_type_id,
				source_carrier,
				i,
				&lower_constructor_index
			) != 0) {
			return -1;
		}
		uint32_t lower_constructor_id = source->first_constructor +
			lower_constructor_index;
		if (lower_constructor_id >= types->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* lower_constructor =
			&types->constructor_declarations[lower_constructor_id];
		uint32_t field_context = empty;
		uint32_t field_faces[64][9];
		for (uint32_t j = 0; j < original_field_count; ++j) {
			const struct prototype_context* original_field =
				prototype_context_get(contexts, original_fields[j]);
			if (!original_field) {
				return -1;
			}
			uint32_t field_carrier =
				prototype_context_classifier_term(original_field);
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous =
					prototype_context_get(contexts, original_fields[k]);
				if (!previous || prototype_term_contains_free_binding(
						terms, field_carrier, previous->binding_id
					)) {
					return -1;
				}
			}
			uint32_t lower_field_identity_id = source_type_id;
			uint32_t higher_field_identity_id = generated_type_id;
			if (field_carrier != source_carrier) {
				if (hott_generated_identity_declaration(
						types,
						field_carrier,
						empty,
						&lower_field_identity_id
					) != 0) {
					return -1;
				}
				uint32_t lower_field_former;
				if (prototype_term_type_instance_make(
						terms,
						types,
						lower_field_identity_id,
						NULL,
						0,
						&lower_field_former
					) != 0 || prototype_type_declaration_add_generated_identity(
						types,
						lower_field_former,
						empty,
						&higher_field_identity_id
					) != 0 || hott_initialize_indexed_higher_identity_declaration(
						terms,
						types,
						contexts,
						lower_field_identity_id,
						higher_field_identity_id,
						universe
					) != 0) {
					return -1;
				}
			}
			uint32_t field_bindings[9];
			uint32_t field_classifiers[9];
			field_classifiers[0] = field_carrier;
			field_classifiers[1] = field_carrier;
			for (uint32_t k = 0; k < 9; ++k) {
				if (k == 2 || k == 5 || k == 6 || k == 7) {
					uint32_t left = k == 2 ? 0 : k == 5 ? 3 :
						k == 6 ? 0 : 1;
					uint32_t right = k == 2 ? 1 : k == 5 ? 4 :
						k == 6 ? 3 : 4;
					uint32_t endpoints[2] = {
						field_faces[j][left], field_faces[j][right]
					};
					if (prototype_term_type_instance_make(
							terms,
							types,
							lower_field_identity_id,
							endpoints,
							2,
							&field_classifiers[k]
						) != 0) {
						return -1;
					}
				} else if (k == 3 || k == 4) {
					field_classifiers[k] = field_carrier;
				} else if (k == 8) {
					if (prototype_term_type_instance_make(
							terms,
							types,
							higher_field_identity_id,
							field_faces[j],
							8,
							&field_classifiers[k]
						) != 0) {
						return -1;
					}
				}
				field_bindings[k] = prototype_term_new_binding(terms);
				uint32_t next_context;
				if (field_bindings[k] == PROTOTYPE_INVALID_ID ||
					prototype_context_extend(
						contexts,
						field_context,
						field_bindings[k],
						field_classifiers[k],
						PROTOTYPE_INVALID_ID,
						&next_context
					) != 0 || prototype_term_var(
						terms, field_bindings[k], &field_faces[j][k]
					) != 0) {
					return -1;
				}
				field_context = next_context;
			}
		}
		uint32_t boundary[8];
		for (uint32_t k = 0; k < 8; ++k) {
			if (k == 0 || k == 1 || k == 3 || k == 4) {
				if (prototype_term_constructor(
						terms, source_carrier, i, &boundary[k]
					) != 0) {
					return -1;
				}
				uint32_t face = k == 0 ? 0 : k == 1 ? 1 :
					k == 3 ? 3 : 4;
				for (uint32_t j = 0; j < original_field_count; ++j) {
					if (prototype_term_app(
							terms,
							boundary[k],
							field_faces[j][face],
							&boundary[k]
						) != 0) {
						return -1;
					}
				}
			} else {
				if (prototype_term_constructor(
						terms,
						lower_constructor->result_classifier,
						lower_constructor_index,
						&boundary[k]
					) != 0) {
					return -1;
				}
				uint32_t left = k == 2 ? 0 : k == 5 ? 3 :
					k == 6 ? 0 : 1;
				uint32_t right = k == 2 ? 1 : k == 5 ? 4 :
					k == 6 ? 3 : 4;
				uint32_t relation = k == 2 ? 2 : k == 5 ? 5 :
					k == 6 ? 6 : 7;
				for (uint32_t j = 0; j < original_field_count; ++j) {
					uint32_t faces[3] = {
						field_faces[j][left],
						field_faces[j][right],
						field_faces[j][relation]
					};
					for (uint32_t l = 0; l < 3; ++l) {
						if (prototype_term_app(
								terms,
								boundary[k],
								faces[l],
								&boundary[k]
							) != 0) {
							return -1;
						}
					}
				}
			}
		}
		uint32_t result_classifier;
		uint32_t curried_classifier;
		if (prototype_term_type_instance_make(
				terms,
				types,
				generated_type_id,
				boundary,
				8,
				&result_classifier
			) != 0 || prototype_type_constructor_derive_curried_classifier(
				terms,
				contexts,
				empty,
				field_context,
				result_classifier,
				&curried_classifier
			) != 0) {
			return -1;
		}
		uint32_t readback_fields[576];
		uint32_t generated_field_count = original_field_count * 9;
		for (uint32_t j = 0; j < generated_field_count; ++j) {
			readback_fields[j] = PROTOTYPE_INVALID_ID;
		}
		uint32_t constructor_id;
		if (prototype_type_declaration_add_constructor(
				types,
				generated_type_id,
				-1,
				readback_fields,
				generated_field_count,
				PROTOTYPE_INVALID_ID,
				empty,
				field_context,
				result_classifier,
				curried_classifier,
				&constructor_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int hott_add_thunk_return_identity_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	uint32_t result_carrier,
	uint32_t result_identity_type_id,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated
) {
	if (!terms || !type_declarations || !contexts || !generated ||
		generated->constructor_count != 0) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t relation_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t right_context;
	uint32_t field_context;
	uint32_t left_value;
	uint32_t right_value;
	uint32_t value_identity;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID ||
		relation_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			generated->parameter_context,
			left_binding,
			result_carrier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			result_carrier,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_term_var(
			terms, left_binding, &left_value
		) != 0 || prototype_term_var(
			terms, right_binding, &right_value
		) != 0) {
		return -1;
	}
	uint32_t value_identity_arguments[2] = { left_value, right_value };
	if (result_identity_type_id != PROTOTYPE_INVALID_ID) {
		if (prototype_term_type_instance_make(
				terms,
				type_declarations,
				result_identity_type_id,
				value_identity_arguments,
				2,
				&value_identity
			) != 0) {
			return -1;
		}
	} else {
		uint32_t x0_binding = prototype_term_new_binding(terms);
		uint32_t x1_binding = prototype_term_new_binding(terms);
		uint32_t xr_binding = prototype_term_new_binding(terms);
		if (x0_binding == PROTOTYPE_INVALID_ID ||
			x1_binding == PROTOTYPE_INVALID_ID ||
			xr_binding == PROTOTYPE_INVALID_ID ||
			hott_build_nondependent_pi_identity_type(
				terms,
				type_declarations,
				generated->parameter_context,
				result_carrier,
				left_binding,
				right_binding,
				x0_binding,
				x1_binding,
				xr_binding,
				&value_identity
			) != 0) {
			return -1;
		}
	}
	if (prototype_context_extend(
			contexts,
			right_context,
			relation_binding,
			value_identity,
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0) {
		return -1;
	}
	uint32_t left_return;
	uint32_t right_return;
	uint32_t left_thunk;
	uint32_t right_thunk;
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_return(
			terms, left_value, &left_return
		) != 0 || prototype_term_return(
			terms, right_value, &right_return
		) != 0 || prototype_term_thunk(
			terms, left_return, &left_thunk
		) != 0 || prototype_term_thunk(
			terms, right_return, &right_thunk
		) != 0) {
		return -1;
	}
	uint32_t result_arguments[2] = { left_thunk, right_thunk };
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			field_context,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[3] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	uint32_t constructor_id;
	if (prototype_type_declaration_add_constructor(
			type_declarations,
			generated_type_id,
			-1,
			readback_fields,
			3,
			PROTOTYPE_INVALID_ID,
			generated->parameter_context,
			field_context,
			result_classifier,
			curried_classifier,
			&constructor_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_certify_homogeneous_endpoint_contexts(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	uint32_t left_context,
	uint32_t right_context,
	uint32_t* p_left_certificate_id,
	uint32_t* p_right_certificate_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_type_claim_id) : NULL;
	const struct prototype_context* left = prototype_context_get(
		contexts, left_context
	);
	const struct prototype_context* right = prototype_context_get(
		contexts, right_context
	);
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !left || !right || !p_left_certificate_id ||
		!p_right_certificate_id || source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		left->parent != source->context_id || right->parent != left_context ||
		prototype_context_classifier_term(left) != source->subject ||
		prototype_context_classifier_term(right) != source->subject) {
		return -1;
	}
	uint32_t left_projection;
	uint32_t right_type_claim;
	if (prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			source_type_claim_id,
			p_left_certificate_id
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			source->context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			left_projection,
			&right_type_claim
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			right_context,
			right_type_claim,
			p_right_certificate_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_publish_generated_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	uint32_t generated_type_id,
	uint32_t expected_constructor_count,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_type_declaration* generated = type_declarations &&
		generated_type_id < type_declarations->type_count ?
		&type_declarations->type_declarations[generated_type_id] : NULL;
	if (!actions || !terms || !contexts || !judgement || !source || !generated ||
		!p_result_id || generated->constructor_count != expected_constructor_count) {
		return -1;
	}
	uint32_t identity_type_former;
	uint32_t identity_type_former_claim;
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			NULL,
			0,
			&identity_type_former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			type_declarations,
			source->context_id,
			identity_type_former,
			generated->formation_classifier,
			&identity_type_former_claim
		) != 0) {
		return -1;
	}
	uint32_t endpoint_path[2];
	uint32_t endpoint_count;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_context_extension_path(
			contexts,
			generated->parameter_context,
			generated->index_context,
			endpoint_path,
			2,
			&endpoint_count
		) != 0 || endpoint_count != 2 || prototype_term_var(
			terms,
			prototype_context_get(contexts, endpoint_path[0])->binding_id,
			&left_endpoint
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, endpoint_path[1])->binding_id,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t endpoint_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			endpoint_arguments,
			2,
			&identity_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			type_declarations,
			generated->index_context,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0 || !request || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		hott_certify_homogeneous_endpoint_contexts(
			kernel,
			request->key.identity_type.source_claim_id,
			endpoint_path[0],
			generated->index_context,
			&left_context_certificate,
			&right_context_certificate
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			generated->index_context,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = generated->index_context,
			.left_endpoint_binding_id =
				prototype_context_get(contexts, endpoint_path[0])->binding_id,
			.right_endpoint_binding_id =
				prototype_context_get(contexts, endpoint_path[1])->binding_id,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = identity_type_former,
			.backing_type_former_has_type_claim_id =
				identity_type_former_claim,
				.identity_type_term_id = identity_type,
				.identity_type_has_type_claim_id = identity_type_claim,
				.identity_type_is_type_claim_id = identity_type_is_type_claim,
				.left_context_certificate_id = left_context_certificate,
				.right_context_certificate_id = right_context_certificate,
				.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_term_is_constant_over_context(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t term_id
) {
	if (!terms || !contexts || term_id >= terms->term_count) {
		return 0;
	}
	uint32_t current = context_id;
	for (uint32_t depth = 0; depth <= contexts->context_count; ++depth) {
		const struct prototype_context* context = prototype_context_get(
			contexts, current
		);
		if (!context) {
			return 0;
		}
		if (current == prototype_context_empty(contexts)) {
			return 1;
		}
		if (prototype_term_contains_free_binding(
				terms, term_id, context->binding_id
			)) {
			return 0;
		}
		current = context->parent;
	}
	return 0;
}

struct hott_identity_endpoint_setup {
	uint32_t left_type_claim_id;
	uint32_t right_type_claim_id;
	uint32_t left_type_term_id;
	uint32_t right_type_term_id;
	uint32_t left_binding_id;
	uint32_t right_binding_id;
	uint32_t left_context_id;
	uint32_t endpoint_context_id;
	uint32_t left_context_certificate_id;
	uint32_t right_context_certificate_id;
};

static int hott_prepare_identity_endpoints(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	const struct prototype_hott_bridge* bridge,
	struct hott_identity_endpoint_setup* p_setup
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, source_type_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !bridge || !p_setup || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->context_id != bridge->source_context_id) {
		return -1;
	}
	memset(p_setup, 0, sizeof(*p_setup));
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->left_substitution_id,
			&p_setup->left_type_claim_id
		) != 0) {
		return -1;
	}
	uint32_t right_type_base_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(
			judgement, p_setup->left_type_claim_id
		);
	const struct prototype_judgement_proposition* right_type =
		prototype_judgement_claim_proposition(judgement, right_type_base_claim);
	if (!left_type || !right_type) {
		return -1;
	}
	p_setup->left_type_term_id = left_type->subject;
	p_setup->right_type_term_id = right_type->subject;
	p_setup->left_binding_id = prototype_term_new_binding(terms);
	p_setup->right_binding_id = prototype_term_new_binding(terms);
	uint32_t left_projection;
	if (p_setup->left_binding_id == PROTOTYPE_INVALID_ID ||
		p_setup->right_binding_id == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			p_setup->left_binding_id,
			p_setup->left_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->left_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->left_context_id,
			p_setup->left_type_claim_id,
			&p_setup->left_context_certificate_id
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			p_setup->left_context_id,
			bridge->bridge_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&p_setup->right_type_claim_id
		) != 0 || prototype_context_extend(
			contexts,
			p_setup->left_context_id,
			p_setup->right_binding_id,
			p_setup->right_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->endpoint_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->endpoint_context_id,
			p_setup->right_type_claim_id,
			&p_setup->right_context_certificate_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_publish_indexed_higher_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_identity_type_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_bridge_certificate* bridge_certificate = bridge &&
		bridges && bridge->id < bridges->certificate_count ?
		&bridges->certificates[bridge->id] : NULL;
	const struct prototype_hott_bridge* parent = bridge_certificate ?
		prototype_hott_bridge_db_get(
			bridges, bridge_certificate->parent_bridge_id
		) : NULL;
	const struct prototype_type_declaration* source_identity = types &&
		source_identity_type_id < types->type_count ?
		&types->type_declarations[source_identity_type_id] : NULL;
	if (!actions || !terms || !types || !contexts || !substitutions ||
		!judgement || !source || !bridge || !bridge_certificate || !parent ||
		!source_identity || !p_result_id || bridge_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		source_identity->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source_identity->parameter_context != prototype_context_empty(contexts) ||
		source_identity->index_count != 2 ||
		source_identity->constructor_count == 0) {
		return -1;
	}
	uint32_t source_type_id;
	uint32_t source_arguments[2];
	uint32_t source_argument_count;
	uint32_t source_former;
	if (prototype_term_type_instance_info(
			terms,
			source->subject,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_type_id != source_identity_type_id ||
		source_argument_count != 2 || prototype_term_type_instance_make(
			terms,
			types,
			source_identity_type_id,
			NULL,
			0,
			&source_former
		) != 0) {
		return -1;
	}
	uint32_t generated_type_id;
	uint32_t empty = prototype_context_empty(contexts);
	if (prototype_type_declaration_add_generated_identity(
			types, source_former, empty, &generated_type_id
		) != 0 || hott_initialize_indexed_higher_identity_declaration(
			terms,
			types,
			contexts,
			source_identity_type_id,
			generated_type_id,
			source->classifier
		) != 0) {
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		return -1;
	}
	uint32_t boundary[8];
	if (prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[0],
			bridge->left_substitution_id,
			&boundary[0]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[1],
			bridge->left_substitution_id,
			&boundary[1]
		) != 0 || prototype_term_var(
			terms, endpoints.left_binding_id, &boundary[2]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[0],
			bridge->right_substitution_id,
			&boundary[3]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[1],
			bridge->right_substitution_id,
			&boundary[4]
		) != 0 || prototype_term_var(
			terms, endpoints.right_binding_id, &boundary[5]
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, parent->bridge_context_id)->binding_id,
			&boundary[6]
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, bridge->bridge_context_id)->binding_id,
			&boundary[7]
		) != 0) {
		return -1;
	}
	const struct prototype_type_declaration* generated =
		&types->type_declarations[generated_type_id];
	uint32_t higher_former;
	uint32_t higher_former_claim;
	uint32_t higher_identity;
	uint32_t higher_identity_claim;
	uint32_t higher_identity_is_type_claim;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &higher_former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			bridge->bridge_context_id,
			higher_former,
			generated->formation_classifier,
			&higher_former_claim
		) != 0 || prototype_term_type_instance_make(
			terms, types, generated_type_id, boundary, 8, &higher_identity
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			endpoints.endpoint_context_id,
			higher_identity,
			source->classifier,
			&higher_identity_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			higher_identity,
			source->classifier,
			higher_identity_claim,
			&higher_identity_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule =
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = higher_former,
			.backing_type_former_has_type_claim_id = higher_former_claim,
			.identity_type_term_id = higher_identity,
			.identity_type_has_type_claim_id = higher_identity_claim,
			.identity_type_is_type_claim_id = higher_identity_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_publish_constant_generated_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_type_declaration* generated = types &&
		generated_type_id < types->type_count ?
		&types->type_declarations[generated_type_id] : NULL;
	if (!actions || !terms || !types || !contexts || !substitutions || !cwf ||
		!judgement || !request || !source || !bridge || !generated ||
		!p_result_id || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		bridge->source_context_id != source->context_id ||
		bridge->source_context_id == prototype_context_empty(contexts) ||
		bridges->certificates[bridge->id].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		!hott_term_is_constant_over_context(
			terms, contexts, source->context_id, source->subject
		) || generated->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		generated->origin_source_carrier_term_id != source->subject ||
		generated->parameter_count != 0 || generated->index_count != 2 ||
		(computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT &&
		 computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE)) {
		return -1;
	}
	uint32_t left_type_claim;
	uint32_t right_type_base_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.identity_type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.identity_type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(judgement, left_type_claim);
	const struct prototype_judgement_proposition* right_type_base =
		prototype_judgement_claim_proposition(judgement, right_type_base_claim);
	if (!left_type || !right_type_base || left_type->subject != source->subject ||
		right_type_base->subject != source->subject) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t right_type_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type->subject,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			left_type_claim,
			&left_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			bridge->bridge_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&right_type_claim
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			endpoint_context,
			right_type_claim,
			&right_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t former;
	uint32_t former_claim;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t endpoints[2];
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			bridge->bridge_context_id,
			former,
			generated->formation_classifier,
			&former_claim
		) != 0 || prototype_term_var(
			terms, left_binding, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_binding, &right_endpoint
		) != 0) {
		return -1;
	}
	endpoints[0] = left_endpoint;
	endpoints[1] = right_endpoint;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, endpoints, 2, &identity_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			endpoint_context,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoint_context,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = former,
			.backing_type_former_has_type_claim_id = former_claim,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_build_nondependent_pi_identity_type(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t source_carrier,
	uint32_t left_endpoint_binding,
	uint32_t right_endpoint_binding,
	uint32_t x0_binding,
	uint32_t x1_binding,
	uint32_t xr_binding,
	uint32_t* p_identity_type
) {
	if (!terms || !type_declarations || !p_identity_type ||
		source_carrier >= terms->term_count) {
		return -1;
	}
	int is_computation_function = terms->terms[source_carrier].tag ==
		PROTOTYPE_TERM_THUNK_TYPE;
	uint32_t pi = is_computation_function ?
		terms->terms[source_carrier].as.thunk_type.computation : source_carrier;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return -1;
	}
	uint32_t domain = terms->terms[pi].as.pi.domain;
	uint32_t codomain_binding;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		prototype_term_contains_free_binding(terms, codomain, codomain_binding) ||
		(is_computation_function ?
			(terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			 prototype_term_effect_row_purity(
				terms, terms->terms[codomain].as.computation_type.label
			 ) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) :
			terms->terms[codomain].tag != PROTOTYPE_TERM_UNIVERSE_VAR)) {
		return -1;
	}
	uint32_t domain_identity_type_id;
	uint32_t codomain_thunk;
	uint32_t codomain_identity_type_id;
	if (hott_generated_identity_declaration(
			type_declarations,
			domain,
			parameter_context,
			&domain_identity_type_id
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk_type(terms, codomain, &codomain_thunk) != 0) {
			return -1;
		}
	} else {
		codomain_thunk = codomain;
	}
	if (hott_generated_identity_declaration(
			type_declarations,
			codomain_thunk,
			parameter_context,
			&codomain_identity_type_id
		) != 0) {
		return -1;
	}
	uint32_t u0;
	uint32_t u1;
	uint32_t x0;
	uint32_t x1;
	uint32_t domain_identity;
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, left_endpoint_binding, &u0
		) != 0 || prototype_term_var(
			terms, right_endpoint_binding, &u1
		) != 0 || prototype_term_var(
			terms, x0_binding, &x0
		) != 0 || prototype_term_var(
			terms, x1_binding, &x1
		) != 0) {
		return -1;
	}
	uint32_t domain_identity_arguments[2] = { x0, x1 };
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			domain_identity_type_id,
			domain_identity_arguments,
			2,
			&domain_identity
		) != 0) {
		return -1;
	}
	uint32_t forced_u0;
	uint32_t forced_u1;
	uint32_t applied_u0;
	uint32_t applied_u1;
	uint32_t thunked_applied_u0;
	uint32_t thunked_applied_u1;
	uint32_t result_identity;
	if (is_computation_function) {
		if (prototype_term_force(terms, u0, &forced_u0) != 0 ||
			prototype_term_force(terms, u1, &forced_u1) != 0) {
			return -1;
		}
	} else {
		forced_u0 = u0;
		forced_u1 = u1;
	}
	if (prototype_term_app(
			terms, forced_u0, x0, &applied_u0
		) != 0 || prototype_term_app(
			terms, forced_u1, x1, &applied_u1
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk(terms, applied_u0, &thunked_applied_u0) != 0 ||
			prototype_term_thunk(terms, applied_u1, &thunked_applied_u1) != 0) {
			return -1;
		}
	} else {
		/* A type-family application is pure type computation. Normalize its
		 * endpoint before materializing the Universe identity TypeView so the
		 * typed source and its erased core describe the same fiber. */
		if (prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u0,
				&thunked_applied_u0
			) != 0 || prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u1,
				&thunked_applied_u1
			) != 0) {
			return -1;
		}
	}
	uint32_t result_identity_arguments[2] = {
		thunked_applied_u0, thunked_applied_u1
	};
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			codomain_identity_type_id,
			result_identity_arguments,
			2,
			&result_identity
		) != 0) {
		return -1;
	}
	uint32_t classifier = result_identity;
	uint32_t binders[3] = { x0_binding, x1_binding, xr_binding };
	uint32_t domains[3] = { domain, domain, domain_identity };
	for (uint32_t i = 3; i > 0; --i) {
		uint32_t family;
		uint32_t pi_classifier;
		uint32_t family_body = classifier;
		if (is_computation_function) {
			uint32_t empty_effects;
			if (prototype_term_effect_row_empty(terms, &empty_effects
				) != 0 || prototype_term_computation_type(
					terms, empty_effects, classifier, &family_body
				) != 0) {
				return -1;
			}
		}
		if (prototype_term_pure_family(
				terms, binders[i - 1], family_body, &family
			) != 0 || prototype_term_pi_family(
				terms, domains[i - 1], family, &pi_classifier
			) != 0 || (is_computation_function && prototype_term_thunk_type(
				terms, pi_classifier, &classifier
			) != 0)) {
			return -1;
		}
		if (!is_computation_function) {
			classifier = pi_classifier;
		}
	}
	*p_identity_type = classifier;
	return 0;
}

static int hott_publish_pi_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !contexts || !judgement || !source || !bridge ||
		!p_result_id || bridge->source_context_id != source->context_id ||
		(computation_rule != PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		 computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT)) {
		return -1;
	}
	if (computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT &&
		(source->context_id == prototype_context_empty(contexts) ||
		 !hott_term_is_constant_over_context(
			terms, contexts, source->context_id, source->subject
		 ))) {
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		return -1;
	}
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t x0_binding = prototype_term_new_binding(terms);
	uint32_t x1_binding = prototype_term_new_binding(terms);
	uint32_t xr_binding = prototype_term_new_binding(terms);
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (hott_build_nondependent_pi_identity_type(
			terms,
			kernel->type_declarations,
			prototype_context_empty(contexts),
			source->subject,
			endpoints.left_binding_id,
			endpoints.right_binding_id,
			x0_binding,
			x1_binding,
			xr_binding,
			&identity_type
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_term_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_has_type_claim_id = PROTOTYPE_INVALID_ID,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = x0_binding,
			.pointwise_right_input_binding_id = x1_binding,
			.pointwise_input_identity_binding_id = xr_binding
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_add_contextual_adt_identity_constructor(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !source_type || !generated || !p_constructor_id ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			types->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&types->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t bridge_id;
	int residual_reason;
	int bridge_status = prototype_hott_bridge_db_ensure_identity_context(
		bridges,
		kernel,
		actions,
		source_constructor->field_context,
		&bridge_id,
		&residual_reason
	);
	if (bridge_status != 0) {
		return bridge_status > 0 ? 1 : -1;
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, bridge_id
	);
	if (!bridge || bridge->source_context_id !=
			source_constructor->field_context || bridge->bridge_context_id >=
			contexts->context_count) {
		return -1;
	}

	uint32_t source_fields[64];
	uint32_t source_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0) {
		return -1;
	}
	uint32_t source_endpoint;
	if (prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &source_endpoint
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* field = prototype_context_get(
			contexts, source_fields[i]
		);
		uint32_t field_term;
		if (!field || prototype_term_var(
				terms, field->binding_id, &field_term
			) != 0 || prototype_term_app(
				terms, source_endpoint, field_term, &source_endpoint
			) != 0) {
			return -1;
		}
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_endpoint,
			bridge->left_substitution_id,
			&left_endpoint
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_endpoint,
			bridge->right_substitution_id,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t result_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			bridge->bridge_context_id,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t generated_fields[192];
	uint32_t generated_field_count;
	if (prototype_context_extension_path(
			contexts,
			generated->parameter_context,
			bridge->bridge_context_id,
			generated_fields,
			192,
			&generated_field_count
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[192];
	for (uint32_t i = 0; i < generated_field_count; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		types,
		generated_type_id,
		-1,
		readback_fields,
		generated_field_count,
		PROTOTYPE_INVALID_ID,
		generated->parameter_context,
		bridge->bridge_context_id,
		result_classifier,
		curried_classifier,
		p_constructor_id
	);
}

static int hott_add_ordinary_adt_identity_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	if (!terms || !type_declarations || !contexts || !source_type || !generated ||
		!p_constructor_id || constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t source_fields[64];
	uint32_t source_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0) {
		return -1;
	}
	uint32_t generated_field_context = generated->parameter_context;
	uint32_t left_fields[64];
	uint32_t right_fields[64];
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* source_field =
			prototype_context_get(contexts, source_fields[i]);
		if (!source_field) {
			return -1;
		}
		uint32_t field_classifier =
			prototype_context_classifier_term(source_field);
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_context* previous =
				prototype_context_get(contexts, source_fields[j]);
			if (!previous || prototype_term_contains_free_binding(
					terms, field_classifier, previous->binding_id
				)) {
				return 1;
			}
		}
		uint32_t field_identity_type_id = generated_type_id;
		if (field_classifier != source_carrier &&
			hott_generated_identity_declaration(
				type_declarations,
				field_classifier,
				generated->parameter_context,
				&field_identity_type_id
			) != 0) {
			return 1;
		}
		uint32_t left_binding = prototype_term_new_binding(terms);
		uint32_t right_binding = prototype_term_new_binding(terms);
		uint32_t relation_binding = prototype_term_new_binding(terms);
		uint32_t left_context;
		uint32_t right_context;
		uint32_t relation_context;
		uint32_t relation_type;
		if (left_binding == PROTOTYPE_INVALID_ID ||
			right_binding == PROTOTYPE_INVALID_ID ||
			relation_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
				contexts, generated_field_context, left_binding, field_classifier,
				PROTOTYPE_INVALID_ID, &left_context
			) != 0 || prototype_context_extend(
				contexts, left_context, right_binding, field_classifier,
				PROTOTYPE_INVALID_ID, &right_context
			) != 0 || prototype_term_var(
				terms, left_binding, &left_fields[i]
			) != 0 || prototype_term_var(
				terms, right_binding, &right_fields[i]
			) != 0) {
			return -1;
		}
		uint32_t relation_arguments[2] = {
			left_fields[i], right_fields[i]
		};
		if (prototype_term_type_instance_make(
				terms, type_declarations, field_identity_type_id,
				relation_arguments, 2, &relation_type
			) != 0 || prototype_context_extend(
				contexts, right_context, relation_binding, relation_type,
				PROTOTYPE_INVALID_ID, &relation_context
			) != 0) {
			return -1;
		}
		generated_field_context = relation_context;
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &left_endpoint
		) != 0 || prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &right_endpoint
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		if (prototype_term_app(
				terms, left_endpoint, left_fields[i], &left_endpoint
			) != 0 || prototype_term_app(
				terms, right_endpoint, right_fields[i], &right_endpoint
			) != 0) {
			return -1;
		}
	}
	uint32_t result_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id, result_arguments, 2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms, contexts, generated->parameter_context, generated_field_context,
			result_classifier, &curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[192];
	uint32_t generated_field_count = source_field_count * 3;
	for (uint32_t i = 0; i < generated_field_count; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		type_declarations, generated_type_id, -1, readback_fields,
		generated_field_count, PROTOTYPE_INVALID_ID,
		generated->parameter_context, generated_field_context, result_classifier,
		curried_classifier, p_constructor_id
	);
}

int prototype_parametricity_execute_relation_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_judgement_claim* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
		prototype_judgement_claim_get(
			judgement, request->key.relation_type.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.relation_type.source_bridge_id
		) : NULL;
	if (!source || !bridge || prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id != bridge->source_context_id) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			judgement,
			request->key.relation_type.source_claim_id,
			&descriptor
		) != 0) {
		return -1;
	}
	if (!descriptor.admitted) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			descriptor.residual_reason,
			p_result_id
		);
	}

	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t endpoint_to_left;
	uint32_t endpoint_projection;
	uint32_t left_type_claim;
	uint32_t right_type_base_claim;
	uint32_t right_type_claim;
	uint32_t endpoint_left_type_claim;
	uint32_t endpoint_type_claim;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t relation_type;
	uint32_t relation_family;
	uint32_t relation_claim;
	uint32_t left_type;
	uint32_t right_type;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_type_evidence =
		prototype_judgement_claim_get(judgement, left_type_claim);
	if (!left_type_evidence) {
		return -1;
	}
	left_type = prototype_judgement_proposition_get(judgement, left_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			left_context,
			left_type_claim,
			&left_context_certificate
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, left_context, &left_projection
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&right_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			left_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* right_type_evidence =
		prototype_judgement_claim_get(judgement, right_type_claim);
	if (!right_type_evidence) {
		return -1;
	}
	right_type = prototype_judgement_proposition_get(judgement, right_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right_type,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			endpoint_context,
			right_type_claim,
			&right_context_certificate
		) != 0 ||
		prototype_substitution_projection_path(
			substitutions,
			contexts,
			endpoint_context,
			bridge->bridge_context_id,
			&endpoint_projection
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, endpoint_context, &endpoint_to_left
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			left_type_claim,
			endpoint_projection,
			&endpoint_left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_claim,
			endpoint_to_left,
			&endpoint_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			right_binding,
			right_type,
			&right_endpoint_claim
		) != 0 ||
		prototype_term_var(terms, left_binding, &left_endpoint) != 0 ||
		prototype_term_var(terms, right_binding, &right_endpoint) != 0 ||
		prototype_term_relation_type(
			terms,
			left_type,
			right_type,
			left_endpoint,
			right_endpoint,
			&relation_type
		) != 0 ||
		prototype_judgement_add_relation_type_formation(
			judgement,
			terms,
			endpoint_context,
			relation_type,
			prototype_judgement_claim_proposition(
				judgement, endpoint_left_type_claim
			)->classifier,
			endpoint_left_type_claim,
			endpoint_type_claim,
			left_endpoint_claim,
			right_endpoint_claim,
			&relation_claim
		) != 0 || prototype_term_pure_family(
			terms,
			right_binding,
			relation_type,
			&relation_family
		) != 0 || prototype_term_pure_family(
			terms,
			left_binding,
			relation_family,
			&relation_family
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE,
		.data.relation_type = {
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.relation_family_semantics =
				PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION,
			.relation_family_term_id = relation_family,
			.relation_type_term_id = relation_type,
			.relation_is_type_claim_id = relation_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges,
			certificate,
			&certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static const struct prototype_judgement_derivation*
hott_derivation_for_claim_and_kind(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
);

static int hott_has_type_premise_for_is_type_claim_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t is_type_claim_id,
	uint32_t remaining_depth,
	uint32_t* p_has_type_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* is_type = judgement ?
		prototype_judgement_claim_proposition(judgement, is_type_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!is_type || !p_has_type_claim_id || remaining_depth == 0 ||
		is_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		return -1;
	}
	uint32_t premise_claim_id;
	if (hott_find_unique_derivation_premise(
			judgement,
			is_type_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
			is_type->subject,
			is_type->classifier,
			&premise_claim_id
		) == 0) {
		*p_has_type_claim_id = premise_claim_id;
		return 0;
	}
	const struct prototype_judgement_derivation* reindex =
		hott_derivation_for_claim_and_kind(
			judgement,
			is_type_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	if (!reindex || reindex->premise_count != 1 ||
		reindex->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		reindex->semantic_action_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t source_has_type_claim_id;
	int source_status = hott_has_type_premise_for_is_type_claim_depth(
		kernel,
		reindex->premises[0].claim_id,
		remaining_depth - 1,
		&source_has_type_claim_id
	);
	if (source_status != 0) {
		return source_status;
	}
	uint32_t reindexed_has_type_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_has_type_claim_id,
			reindex->semantic_action_id,
			&reindexed_has_type_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* reindexed =
		prototype_judgement_claim_proposition(
			judgement, reindexed_has_type_claim_id
		);
	if (!reindexed || reindexed->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		reindexed->context_id != is_type->context_id ||
		reindexed->subject != is_type->subject ||
		reindexed->classifier != is_type->classifier) {
		return -1;
	}
	*p_has_type_claim_id = reindexed_has_type_claim_id;
	return 0;
}

static int hott_construct_type_expression_universe_witness(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_is_type_claim_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t* p_universe_identity_result_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	uint32_t source_has_type_claim_id;
	uint32_t universe_is_type_claim_id;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !source || !bridge ||
		!p_universe_identity_result_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source->classifier >=
			terms->term_count || terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	int has_type_status = hott_has_type_premise_for_is_type_claim_depth(
			kernel,
			source_is_type_claim_id,
			(uint32_t)judgement->claim_count + 1,
			&source_has_type_claim_id
		);
	if (has_type_status != 0) {
		fprintf(
			stderr,
			"universe fiber missing HAS_TYPE premise source_claim=%u status=%d\n",
			source_is_type_claim_id,
			has_type_status
		);
		return has_type_status;
	}
	int universe_status = hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&universe_is_type_claim_id
		);
	if (universe_status != 0) {
		fprintf(
			stderr,
			"universe fiber cannot type Universe source_claim=%u status=%d\n",
			source_is_type_claim_id,
			universe_status
		);
		return universe_status;
	}
	struct prototype_hott_action_request identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_is_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	struct prototype_kernel_view view;
	uint32_t identity_request_id;
	uint32_t identity_result_id;
	int view_status = prototype_kernel_builder_view(kernel, &view);
	int identity_intern_status = view_status == 0 ?
		prototype_hott_action_request_intern(
			actions, &view, bridges, identity_request, &identity_request_id
		) : -1;
	int identity_execute_status = identity_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			actions,
			kernel,
			bridges,
			identity_request_id,
			&identity_result_id
		) : -1;
	if (view_status != 0 || identity_intern_status != 0 ||
		identity_execute_status != 0) {
		fprintf(
			stderr,
			"universe fiber Universe identity failed source_claim=%u "
			"has_type_claim=%u universe_claim=%u view=%d intern=%d execute=%d\n",
			source_is_type_claim_id,
			source_has_type_claim_id,
			universe_is_type_claim_id,
			view_status,
			identity_intern_status,
			identity_execute_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* identity_result =
		prototype_hott_action_result_get(actions, identity_result_id);
	if (!identity_result || identity_result->outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		fprintf(
			stderr,
			"universe fiber Universe identity not ready source_claim=%u "
			"request=%u result=%u state=%d residual=%d\n",
			source_is_type_claim_id,
			identity_request_id,
			identity_result_id,
			identity_result ? identity_result->outcome.state : -1,
			identity_result ? identity_result->outcome.residual_reason : -1
		);
		return identity_result ? 1 : -1;
	}
	struct prototype_hott_action_request object_request = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = source_has_type_claim_id,
			.source_bridge_id = bridge->id,
			.identity_type_action_request_id = identity_request_id
		}
	};
	uint32_t object_request_id;
	uint32_t object_result_id;
	int object_intern_status = prototype_hott_action_request_intern(
			actions, &view, bridges, object_request, &object_request_id
		);
	int object_execute_status = object_intern_status == 0 ?
		prototype_hott_execute_object_term_action(
			actions,
			kernel,
			bridges,
			object_request_id,
			&object_result_id
		) : -1;
	if (object_intern_status != 0 || object_execute_status != 0) {
		fprintf(
			stderr,
			"universe fiber object action failed source_claim=%u "
			"has_type_claim=%u bridge=%u identity_request=%u intern=%d execute=%d\n",
			source_is_type_claim_id,
			source_has_type_claim_id,
			bridge->id,
			identity_request_id,
			object_intern_status,
			object_execute_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* object_result =
		prototype_hott_action_result_get(actions, object_result_id);
	const struct prototype_hott_action_certificate* object_certificate =
		object_result && object_result->outcome.state ==
			PROTOTYPE_HOTT_ACTION_RESULT_READY && object_result->certificate_id <
			actions->certificate_count ?
		&actions->certificates[object_result->certificate_id] : NULL;
	if (!object_certificate || object_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
		return object_result ? 1 : -1;
	}
	*p_universe_identity_result_id = identity_result_id;
	*p_witness_claim_id =
		object_certificate->data.object_term.witness_has_type_claim_id;
	return 0;
}

static int hott_publish_universe_fiber_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source_context = contexts && source ?
		prototype_context_get(contexts, source->context_id) : NULL;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (!actions || !terms || !types || !contexts || !substitutions || !judgement ||
		!source || !bridge || !source_context || !bridges || !request ||
		!p_result_id || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source->subject >= terms->term_count ||
		terms->terms[source->subject].tag == PROTOTYPE_TERM_UNIVERSE_VAR ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	uint32_t universe_result_id;
	uint32_t universe_witness_claim_id;
	int universe_action_status = hott_construct_type_expression_universe_witness(
		actions,
		kernel,
		bridges,
		request->key.identity_type.source_claim_id,
		source,
		bridge,
		&universe_result_id,
		&universe_witness_claim_id
	);
	if (universe_action_status != 0) {
		return universe_action_status;
	}
	uint32_t relation;
	uint32_t relation_claim;
	int relation_projection_status =
		prototype_hott_construct_universe_correspondence_projection(
			actions,
			kernel,
			bridges,
			universe_result_id,
			universe_witness_claim_id,
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
			&relation,
			&relation_claim
		);
	if (relation_projection_status != 0) {
		fprintf(
			stderr,
			"universe fiber relation projection failed request=%u result=%u "
			"witness_claim=%u status=%d\n",
			request_id,
			universe_result_id,
			universe_witness_claim_id,
			relation_projection_status
		);
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			request->key.identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		fprintf(stderr, "universe fiber endpoint preparation failed request=%u\n", request_id);
		return -1;
	}
	uint32_t endpoint_projection;
	uint32_t relation_in_endpoints;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			endpoints.endpoint_context_id,
			bridge->bridge_context_id,
			&endpoint_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			relation_claim,
			endpoint_projection,
			&relation_in_endpoints
		) != 0 || prototype_term_var(
			terms, endpoints.left_binding_id, &left_endpoint
		) != 0 || prototype_term_var(
			terms, endpoints.right_binding_id, &right_endpoint
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.left_binding_id,
			endpoints.left_type_term_id,
			&left_endpoint_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.right_binding_id,
			endpoints.right_type_term_id,
			&right_endpoint_claim
		) != 0) {
		fprintf(
			stderr,
			"universe fiber endpoint setup failed request=%u endpoint_context=%u "
			"bridge_context=%u relation_claim=%u\n",
			request_id,
			endpoints.endpoint_context_id,
			bridge->bridge_context_id,
			relation_claim
		);
		return -1;
	}
	uint32_t relation_at_left;
	uint32_t relation_at_left_claim;
	uint32_t right_relation_classifier;
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	if (prototype_term_pi(
			terms,
			endpoints.right_type_term_id,
			source->classifier,
			&right_relation_classifier
		) != 0 || prototype_term_app(
			terms, relation, left_endpoint, &relation_at_left
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			relation_at_left,
			right_relation_classifier,
			relation_in_endpoints,
			left_endpoint_claim,
			&relation_at_left_claim
		) != 0 || prototype_term_app(
			terms, relation_at_left, right_endpoint, &identity_type
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			relation_at_left_claim,
			right_endpoint_claim,
			&identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		fprintf(
			stderr,
			"universe fiber relation application failed request=%u relation=%u "
			"relation_claim=%u left_type=%u right_type=%u left_claim=%u "
			"right_claim=%u\n",
			request_id,
			relation,
			relation_in_endpoints,
			endpoints.left_type_term_id,
			endpoints.right_type_term_id,
			left_endpoint_claim,
			right_endpoint_claim
		);
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule =
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_term_id = relation,
			.backing_type_former_has_type_claim_id = relation_claim,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	int final_view_status = prototype_kernel_builder_view(kernel, &view);
	int certificate_status = final_view_status == 0 ?
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) : -1;
	if (final_view_status != 0 || certificate_status != 0) {
		fprintf(
			stderr,
			"universe fiber certificate failed request=%u view=%d add=%d "
			"identity_type=%u identity_claim=%u is_type_claim=%u\n",
			request_id,
			final_view_status,
			certificate_status,
			identity_type,
			identity_type_claim,
			identity_type_is_type_claim
		);
		return -1;
	}
	int publish_status = hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
	if (publish_status != 0) {
		fprintf(
			stderr,
			"universe fiber result publication failed request=%u certificate=%u "
			"status=%d\n",
			request_id,
			certificate_id,
			publish_status
		);
	}
	return publish_status;
}

int prototype_hott_execute_identity_type_computation(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !type_declarations || !contexts || !judgement ||
		!bridges || !p_result_id ||
		prototype_kernel_builder_validate_stores(kernel) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (!request || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION) {
		return -1;
	}
	const struct prototype_judgement_proposition* source =
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		);
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, request->key.identity_type.source_bridge_id
	);
	if (!source || !bridge || source->context_id != bridge->source_context_id) {
		return -1;
	}
	if (source->context_id != prototype_context_empty(contexts)) {
		int universe_fiber_status =
			hott_publish_universe_fiber_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				p_result_id
			);
		if (universe_fiber_status <= 0) {
			return universe_fiber_status;
		}
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			judgement,
			request->key.identity_type.source_claim_id,
			&descriptor
		) != 0) {
		fprintf(
			stderr,
			"identity type descriptor failed request=%u source_claim=%u "
			"context=%u subject=%u classifier=%u\n",
			request_id,
			request->key.identity_type.source_claim_id,
			source->context_id,
			source->subject,
			source->classifier
		);
		return -1;
	}
	if (descriptor.capabilities.identity_computation ==
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	if (descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED) {
		int reason = descriptor.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NONE ? descriptor.residual_reason :
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			reason,
			p_result_id
		);
	}
	if (source->context_id != prototype_context_empty(contexts) &&
		descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE) {
		uint32_t generated_type_id;
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				source->subject,
				prototype_context_empty(contexts),
				&generated_type_id
			) != 0) {
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		return hott_publish_constant_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE,
			p_result_id
		);
	}
	if (source->context_id != prototype_context_empty(contexts)) {
		uint32_t source_type_id;
		uint32_t source_arguments[2];
		uint32_t source_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				source->subject,
				&source_type_id,
				source_arguments,
				&source_argument_count
			) == 0 && source_argument_count == 2 && source_type_id <
				type_declarations->type_count && type_declarations->type_declarations[
					source_type_id
				].origin_kind ==
					PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY) {
			return hott_publish_indexed_higher_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				source_type_id,
				p_result_id
			);
		}
		uint32_t generated_type_id;
		if (!hott_term_is_constant_over_context(
				terms, contexts, source->context_id, source->subject
			)) {
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				source->subject,
				prototype_context_empty(contexts),
				&generated_type_id
			) != 0) {
			const struct prototype_term* source_term =
				&terms->terms[source->subject];
			uint32_t computation = source_term->tag ==
				PROTOTYPE_TERM_THUNK_TYPE ?
				source_term->as.thunk_type.computation : PROTOTYPE_INVALID_ID;
			if (source_term->tag == PROTOTYPE_TERM_PI ||
				(computation < terms->term_count &&
				 terms->terms[computation].tag == PROTOTYPE_TERM_PI)) {
				return hott_publish_pi_identity_result(
					actions,
					kernel,
					bridges,
					request_id,
					source,
					bridge,
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT,
					p_result_id
				);
			}
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		return hott_publish_constant_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE) {
		uint32_t generated_type_id;
		if (prototype_type_declaration_add_generated_identity(
				type_declarations,
				source->subject,
				source->context_id,
				&generated_type_id
			) != 0 || generated_type_id >= type_declarations->type_count) {
			return -1;
		}
		struct prototype_type_declaration* generated =
			&type_declarations->type_declarations[generated_type_id];
		if (hott_initialize_generated_identity_declaration(
				terms,
				contexts,
				source->context_id,
				source->subject,
				source->classifier,
				generated
			) != 0) {
			return -1;
		}
		if (generated->constructor_count == 0) {
			uint32_t constructor_id;
			if (hott_add_universe_correspondence_constructor(
					terms,
					type_declarations,
					contexts,
					source->subject,
					generated_type_id,
					generated,
					&constructor_id
				) != 0) {
				return -1;
			}
		}
		return hott_publish_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			generated_type_id,
			1,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI) {
		return hott_publish_pi_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK) {
		const struct prototype_term* thunk_type =
			&terms->terms[source->subject];
		const struct prototype_term* computation_type =
			&terms->terms[thunk_type->as.thunk_type.computation];
		if (computation_type->tag == PROTOTYPE_TERM_PI) {
			return hott_publish_pi_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
				p_result_id
			);
		}
		if (computation_type->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return -1;
		}
		uint32_t result_identity_type_id = PROTOTYPE_INVALID_ID;
		uint32_t generated_type_id;
		int result_identity_status = hott_generated_identity_declaration(
				type_declarations,
				computation_type->as.computation_type.result,
				prototype_context_empty(contexts),
				&result_identity_type_id
			);
		uint32_t result_carrier = computation_type->as.computation_type.result;
		uint32_t result_computation = result_carrier < terms->term_count &&
			terms->terms[result_carrier].tag == PROTOTYPE_TERM_THUNK_TYPE ?
			terms->terms[result_carrier].as.thunk_type.computation :
			PROTOTYPE_INVALID_ID;
		if ((result_identity_status != 0 &&
			(result_computation >= terms->term_count ||
			 terms->terms[result_computation].tag != PROTOTYPE_TERM_PI)) ||
			prototype_type_declaration_add_generated_identity(
				type_declarations,
				source->subject,
				source->context_id,
				&generated_type_id
			) != 0 || generated_type_id >= type_declarations->type_count) {
			return -1;
		}
		struct prototype_type_declaration* generated =
			&type_declarations->type_declarations[generated_type_id];
		if (hott_initialize_generated_identity_declaration(
				terms,
				contexts,
				source->context_id,
				source->subject,
				source->classifier,
				generated
			) != 0) {
			return -1;
		}
		if (generated->constructor_count == 0 &&
			hott_add_thunk_return_identity_constructor(
				terms,
				type_declarations,
				contexts,
				result_carrier,
				result_identity_type_id,
				generated_type_id,
				generated
			) != 0) {
			return -1;
		}
		return hott_publish_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			generated_type_id,
			1,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			p_result_id
		);
	}
	if (descriptor.kind != PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT) {
		return -1;
	}
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			source->subject,
			&source_type_id,
			&source_type
		) != 0 || !source_type || source_type->parameter_count != 0 ||
		source_type->constructor_count == 0) {
		return -1;
	}
	uint32_t generated_type_id;
	if (prototype_type_declaration_add_generated_identity(
			type_declarations,
			source->subject,
			source->context_id,
			&generated_type_id
		) != 0 || generated_type_id >= type_declarations->type_count) {
		return -1;
	}
	struct prototype_type_declaration* generated =
		&type_declarations->type_declarations[generated_type_id];
	if (hott_initialize_generated_identity_declaration(
			terms,
			contexts,
			source->context_id,
			source->subject,
			source->classifier,
			generated
		) != 0) {
		return -1;
	}
	uint32_t expected_constructor_count = 0;
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		int compatible = hott_indexed_constructor_compatible(
			terms,
			type_declarations,
			contexts,
			source->subject,
			source_type,
			i
		);
		if (compatible < 0) {
			return -1;
		}
		expected_constructor_count += compatible != 0;
	}
	if (generated->constructor_count == 0) {
		for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
			int compatible = hott_indexed_constructor_compatible(
				terms,
				type_declarations,
				contexts,
				source->subject,
				source_type,
				i
			);
			if (compatible < 0) {
				return -1;
			}
			if (compatible == 0) {
				continue;
			}
			uint32_t generated_constructor_id;
			int recursive = hott_constructor_has_recursive_field(
				terms,
				contexts,
				source->subject,
				&type_declarations->constructor_declarations[
					source_type->first_constructor + i
				]
			);
			if (recursive < 0) {
				return -1;
			}
			int constructor_status = recursive ?
				hott_add_ordinary_adt_identity_constructor(
					terms,
					type_declarations,
					contexts,
					source->subject,
					source_type,
					i,
					generated_type_id,
					generated,
					&generated_constructor_id
				) : hott_add_contextual_adt_identity_constructor(
					actions,
					kernel,
					bridges,
					source->subject,
					source_type,
					i,
					generated_type_id,
					generated,
					&generated_constructor_id
				);
			if (constructor_status < 0) {
				fprintf(
					stderr,
					"identity constructor generation failed request=%u source=%u "
					"constructor=%u recursive=%d status=%d\n",
					request_id,
					source->subject,
					i,
					recursive,
					constructor_status
				);
				return -1;
			}
			if (constructor_status > 0) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
					p_result_id
				);
			}
		}
	}
	int publication_status = hott_publish_generated_identity_result(
		actions,
		kernel,
		bridges,
		request_id,
		source,
		generated_type_id,
		expected_constructor_count,
		PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
		p_result_id
	);
	if (publication_status != 0) {
		fprintf(
			stderr,
			"generated identity publication failed request=%u source=%u "
			"generated_type=%u expected_constructors=%u actual_constructors=%u "
			"status=%d\n",
			request_id,
			source->subject,
			generated_type_id,
			expected_constructor_count,
			generated->constructor_count,
			publication_status
		);
	}
	return publication_status;
}

static const struct prototype_hott_bridge*
