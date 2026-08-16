int prototype_judgement_resolve_match_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!terms || !type_declarations || !contexts || !p_resolution ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	uint32_t normalized_classifier;
	if (classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			scrutinee_classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_type_instance_info(
		terms,
		normalized_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}

	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(
			type_declarations,
			type_id,
			constructor_symbol_id
		);
	if (!constructor || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (ignored_arg_count != type->parameter_count + type->index_count) {
		return -1;
	}
	uint32_t constructor_owner;
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			type_id,
			ignored_args,
			type->parameter_count,
			&constructor_owner
		) != 0) {
		return -1;
	}

	uint32_t field_contexts[64];
	uint32_t field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_contexts,
			64,
			&field_count
		) != 0) {
		return -1;
	}

	/* Constructor fields are specialized by family parameters. Indices belong
	 * to the constructor result and are refined by Match typing, so retaining
	 * them in the owner would misapply the declaration telescope. */
	p_resolution->constructor_owner = constructor_owner;
	p_resolution->constructor_id = constructor->constructor_index;
	p_resolution->field_count = field_count;
	return 0;
}

int prototype_judgement_classifier_is_recursive_family_instance(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	int* p_recursive
) {
	if (!terms || !type_declarations || !p_recursive ||
		candidate >= terms->term_count || scrutinee_classifier >= terms->term_count) {
		return -1;
	}
	*p_recursive = 0;
	while (candidate < terms->term_count &&
		terms->terms[candidate].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		candidate = terms->terms[candidate].as.type_view.core;
	}
	while (scrutinee_classifier < terms->term_count &&
		terms->terms[scrutinee_classifier].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		scrutinee_classifier =
			terms->terms[scrutinee_classifier].as.type_view.core;
	}
	if (candidate >= terms->term_count ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}
	uint32_t candidate_type_id;
	uint32_t scrutinee_type_id;
	uint32_t candidate_arguments[16];
	uint32_t scrutinee_arguments[16];
	uint32_t candidate_argument_count;
	uint32_t scrutinee_argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			candidate,
			&candidate_type_id,
			candidate_arguments,
			16,
			&candidate_argument_count
		) != 0 || prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			scrutinee_classifier,
			&scrutinee_type_id,
			scrutinee_arguments,
			16,
			&scrutinee_argument_count
		) != 0) {
		return 0;
	}
	if (candidate_type_id != scrutinee_type_id ||
		candidate_type_id >= type_declarations->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[candidate_type_id];
	if (type->parameter_count > 16 || type->index_count > 16 ||
		candidate_argument_count != type->parameter_count + type->index_count ||
		(scrutinee_argument_count != type->parameter_count &&
		 scrutinee_argument_count != type->parameter_count + type->index_count)) {
		return 0;
	}
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		struct prototype_term_conversion_result conversion =
			prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				candidate_arguments[i],
				scrutinee_arguments[i]
			);
		if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
			return 0;
		}
		if (conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
	}
	*p_recursive = 1;
	return 0;
}

static int recursive_family_occurrence(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	uint32_t depth
) {
	if (!terms || !type_declarations || candidate >= terms->term_count ||
		depth > 128) {
		return -1;
	}
	int direct;
	if (prototype_judgement_classifier_is_recursive_family_instance(
			terms, type_declarations, candidate, scrutinee_classifier, &direct
		) != 0) {
		return -1;
	}
	if (direct) {
		return 1;
	}
	const struct prototype_term* term = &terms->terms[candidate];
	switch (term->tag) {
		case PROTOTYPE_TERM_PI: {
			int domain = recursive_family_occurrence(
				terms, type_declarations, term->as.pi.domain,
				scrutinee_classifier, depth + 1
			);
			uint32_t binder;
			uint32_t codomain;
			if (domain != 0 || prototype_term_pure_family_parts(
					terms, term->as.pi.codomain_family, &binder, &codomain
				) != 0) {
				return domain < 0 ? -1 : 2;
			}
			return recursive_family_occurrence(
				terms, type_declarations, codomain,
				scrutinee_classifier, depth + 1
			);
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return recursive_family_occurrence(
				terms, type_declarations, term->as.computation_type.result,
				scrutinee_classifier, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return recursive_family_occurrence(
				terms, type_declarations, term->as.thunk_type.computation,
				scrutinee_classifier, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return recursive_family_occurrence(
				terms, type_declarations, term->as.effect_row_forall.body,
				scrutinee_classifier, depth + 1
			);
		default:
			return 0;
	}
}

static int lift_recursive_field_classifier_inner(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t field_value,
	uint32_t scrutinee_classifier,
	uint32_t motive,
	uint32_t depth,
	uint32_t* p_classifier
) {
	if (!p_classifier || candidate >= terms->term_count ||
		field_value >= terms->term_count || depth > 128) {
		return -1;
	}
	int direct;
	if (prototype_judgement_classifier_is_recursive_family_instance(
			terms, type_declarations, candidate, scrutinee_classifier, &direct
		) != 0) {
		return -1;
	}
	if (direct) {
		return prototype_judgement_apply_indexed_motive(
			terms, type_declarations, motive, field_value, candidate, 1,
			p_classifier
		);
	}
	const struct prototype_term* term = &terms->terms[candidate];
	switch (term->tag) {
		case PROTOTYPE_TERM_PI: {
			if (recursive_family_occurrence(
					terms, type_declarations, term->as.pi.domain,
					scrutinee_classifier, depth + 1
				) != 0) {
				return -1;
			}
			uint32_t binder;
			uint32_t codomain;
			uint32_t argument;
			uint32_t applied_value;
			uint32_t lifted_codomain;
			uint32_t family;
			if (prototype_term_pure_family_parts(
					terms, term->as.pi.codomain_family, &binder, &codomain
				) != 0 || prototype_term_var(terms, binder, &argument) != 0 ||
				prototype_term_app(
					terms, field_value, argument, &applied_value
				) != 0 || lift_recursive_field_classifier_inner(
					terms, type_declarations, codomain, applied_value,
					scrutinee_classifier, motive, depth + 1, &lifted_codomain
				) != 0 || prototype_term_pure_family(
					terms, binder, lifted_codomain, &family
				) != 0) {
				return -1;
			}
			return prototype_term_pi_family(
				terms, term->as.pi.domain, family, p_classifier
			);
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
			uint32_t lifted;
			if (lift_recursive_field_classifier_inner(
					terms, type_declarations, term->as.computation_type.result,
					field_value, scrutinee_classifier, motive, depth + 1,
					&lifted
				) != 0) {
				return -1;
			}
			struct prototype_term_classifier_view lifted_view;
			if (prototype_judgement_classifier_view(
					terms, type_declarations, NULL, lifted, &lifted_view
				) == 0 && lifted_view.category ==
					PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
				lifted_view.computation_kind ==
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				uint32_t joined_row;
				if (prototype_term_effect_row_union(
						terms, term->as.computation_type.label,
						lifted_view.effect_row, &joined_row
					) != 0) {
					return -1;
				}
				return prototype_term_computation_type(
					terms, joined_row, lifted_view.result, p_classifier
				);
			}
			return prototype_term_computation_type(
				terms, term->as.computation_type.label, lifted, p_classifier
			);
		}
		case PROTOTYPE_TERM_THUNK_TYPE: {
			uint32_t lifted;
			if (lift_recursive_field_classifier_inner(
					terms, type_declarations, term->as.thunk_type.computation,
					field_value, scrutinee_classifier, motive, depth + 1,
					&lifted
				) != 0) {
				return -1;
			}
			return prototype_term_thunk_type(terms, lifted, p_classifier);
		}
		default:
			return -1;
	}
}

int prototype_judgement_classifier_is_strictly_positive_recursive_field(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	int* p_recursive
) {
	if (!p_recursive) {
		return -1;
	}
	int occurrence = recursive_family_occurrence(
		terms, type_declarations, candidate, scrutinee_classifier, 0
	);
	if (occurrence < 0) {
		return -1;
	}
	*p_recursive = occurrence == 1;
	return 0;
}

int prototype_judgement_lift_recursive_field_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t candidate,
	uint32_t field_value,
	uint32_t scrutinee_classifier,
	uint32_t motive,
	uint32_t* p_classifier
) {
	int occurrence = recursive_family_occurrence(
		terms, type_declarations, candidate, scrutinee_classifier, 0
	);
	if (occurrence != 1) {
		return occurrence < 0 ? -1 : 1;
	}
	return lift_recursive_field_classifier_inner(
		terms, type_declarations, candidate, field_value,
		scrutinee_classifier, motive, 0, p_classifier
	);
}

static const struct prototype_type_constructor_declaration* lookup_constructor_for_owner_index(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations ||
		prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count) {
		return NULL;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			return constructor;
		}
	}
	return NULL;
}

static int constructor_classifier_from_schema(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	const struct prototype_type_constructor_declaration* constructor,
	uint32_t* p_classifier
) {
	if (!delta || !delta->contexts || !terms || !type_declarations ||
		!constructor || !p_classifier ||
		owner >= terms->term_count) {
		return 1;
	}

	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count ||
		type_id != constructor->owner_type) {
		return 1;
	}

	uint32_t classifier;
	if (prototype_type_constructor_derive_curried_classifier(
			terms,
			delta->contexts,
			constructor->parameter_context,
			constructor->field_context,
			constructor->result_classifier,
			&classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}

	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)classifier,
			&classifier
		) != 0) {
		return -1;
	}
	if (!classifier_returns_owner(terms, type_declarations, classifier, owner)) {
		return 1;
	}
	*p_classifier = classifier;
	return 0;
}

static int materialize_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	uint32_t* p_constructor_term,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_constructor_term || !p_classifier ||
		owner >= terms->term_count) {
		return -1;
	}

	uint32_t constructor_term;
	if (prototype_term_constructor(
			terms,
			owner,
			constructor_index,
			&constructor_term
		) != 0) {
		return -1;
	}

	uint32_t existing_classifiers[32];
	uint32_t existing_classifier_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			constructor_term,
			existing_classifiers,
			32,
			&existing_classifier_count
		) != 0) {
		return -1;
	}
	uint32_t selected_existing = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < existing_classifier_count; ++i) {
		if (!classifier_returns_owner(terms, type_declarations, existing_classifiers[i], owner)) {
			continue;
		}
		if (selected_existing != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				selected_existing,
				existing_classifiers[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		selected_existing = existing_classifiers[i];
	}
	if (selected_existing != PROTOTYPE_INVALID_ID) {
		*p_constructor_term = constructor_term;
		*p_classifier = selected_existing;
		return 0;
	}

	uint32_t schema_owner;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)owner,
			&schema_owner
		) != 0) {
		return -1;
	}
	uint32_t schema_type_id;
	uint32_t schema_args[16];
	uint32_t schema_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			schema_owner,
			&schema_type_id,
			schema_args,
			&schema_arg_count
		) != 0) {
		if (prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				schema_owner,
				&schema_type_id,
				schema_args,
				16,
				&schema_arg_count
			) != 0 ||
			prototype_term_type_instance_make(
				terms,
				type_declarations,
				schema_type_id,
				schema_args,
				schema_arg_count,
				&schema_owner
			) != 0) {
			return -1;
		}
	}
	const struct prototype_type_constructor_declaration* constructor =
		lookup_constructor_for_owner_index(
			terms,
			type_declarations,
			schema_owner,
			constructor_index
		);
	if (!constructor) {
		return -1;
	}

	uint32_t cached_classifier;
	int cache_status = constructor_classifier_from_schema(
		delta,
		terms,
		type_declarations,
		owner,
		constructor,
		&cached_classifier
	);
	if (cache_status < 0) {
		return -1;
	}
	if (cache_status > 0) {
		return -1;
	}

	if (add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
			constructor_term,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			constructor_term,
			cached_classifier,
			PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO
		) != 0) {
		return -1;
	}
	*p_constructor_term = constructor_term;
	*p_classifier = cached_classifier;
	return 0;
}

int prototype_judgement_constructor_field_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!p_classifier ||
		(previous_binder_count > 0 && !previous_binders)) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			arguments,
			64,
			&argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count != type->parameter_count ||
		constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (previous_binder_count < field_index) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			source_context,
			constructor->parameter_context,
			arguments,
			argument_count,
			&substitution
		) != 0) {
		return -1;
	}
	uint32_t previous_terms[64];
	for (uint32_t i = 0; i < field_index; ++i) {
		if (prototype_term_var(
				terms, previous_binders[i].binding_id, &previous_terms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_telescope_entry_classifier(
		contexts,
		substitutions,
		terms,
		type_declarations,
		substitution,
		constructor->parameter_context,
		constructor->field_context,
		previous_terms,
		field_index,
		field_index,
		p_classifier
	);
}

static int constructor_field_classifier_from_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta) {
		return -1;
	}
	return prototype_judgement_constructor_field_classifier(
		terms,
		type_declarations,
		delta->contexts,
		delta->substitutions,
		delta->current_context_id,
		owner,
		constructor_index,
		previous_binders,
		previous_binder_count,
		field_index,
		p_classifier
	);
}

int prototype_judgement_constructor_spine_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t subject,
	uint32_t constructor_owner_view,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count,
	uint32_t* p_classifier,
	int* p_saturated
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!p_classifier || !p_saturated ||
		(argument_classifier_count > 0 && !argument_classifiers)) {
		return -1;
	}
	uint32_t constructor_head;
	uint32_t structural_owner;
	uint32_t constructor_index;
	uint32_t field_terms[64];
	uint32_t field_count;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&constructor_head,
			&structural_owner,
			&constructor_index,
			field_terms,
			64,
			&field_count
		) != 0 || field_count != argument_classifier_count) {
		return -1;
	}
	uint32_t owner = structural_owner;
	if (constructor_owner_view != PROTOTYPE_INVALID_ID) {
		int same_core;
		if (constructor_owner_view >= terms->term_count ||
			prototype_term_core_shape_equal(
				terms, structural_owner, constructor_owner_view, &same_core
			) != 0 || !same_core) {
			return -1;
		}
		owner = constructor_owner_view;
	}
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			owner_arguments,
			64,
			&owner_argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count ||
		owner_argument_count != type->parameter_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	uint32_t field_context_path[64];
	uint32_t declared_field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_context_path,
			64,
			&declared_field_count
		) != 0 || field_count > declared_field_count) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			source_context,
			constructor->parameter_context,
			owner_arguments,
			owner_argument_count,
			&substitution
		) != 0) {
		return -1;
	}

	uint32_t residual_classifier;
	if (prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			constructor->result_classifier,
			&residual_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		if (prototype_term_app(
				terms, residual_classifier, owner_arguments[i], &residual_classifier
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			residual_classifier,
			&residual_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		const struct prototype_context* field_context =
			prototype_context_get(contexts, field_context_path[i]);
		uint32_t expected_classifier;
		uint32_t normalized_expected;
		uint32_t next_residual;
		if (!field_context) {
			return -1;
		}
		uint32_t field_classifier =
			prototype_context_classifier_term(field_context);
		if (prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				field_classifier,
				substitution,
				&expected_classifier
			) != 0) {
			return -1;
		}
		if (prototype_judgement_classifier_value_whnf(
				terms, type_declarations, expected_classifier, &normalized_expected
			) != 0) {
			return -1;
		}
		uint32_t solved_expected;
		if (prototype_judgement_solve_expected_effect_rows(
				terms,
				type_declarations,
				NULL,
				normalized_expected,
				argument_classifiers[i],
				&solved_expected
			) != 0) {
			return PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_DOMAIN_MISMATCH;
		}
		(void)solved_expected;
		if (pi_codomain_after_argument_in_context(
				contexts,
				substitutions,
				terms,
				type_declarations,
				source_context,
				residual_classifier,
				field_terms[i],
				argument_classifiers[i],
				&next_residual
			) != 0) {
			return -1;
		}
		if (prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				field_context_path[i],
				field_terms[i],
				argument_classifiers[i],
				&substitution
			) != 0) {
			return -1;
		}
		residual_classifier = next_residual;
	}

	uint32_t classifier = residual_classifier;
	*p_saturated = field_count == declared_field_count;
	if (*p_saturated && prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			constructor->result_classifier,
			substitution,
			&classifier
		) != 0) {
		return -1;
	}
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, classifier, &classifier
		) != 0) {
		return -1;
	}
	(void)constructor_head;
	*p_classifier = classifier;
	return 0;
}

int prototype_judgement_constructor_spine_expected_domains(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t subject,
	uint32_t constructor_owner_view,
	uint32_t* expected_domains,
	uint32_t expected_domain_capacity,
	uint32_t* p_domain_count
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!expected_domains || !p_domain_count) {
		return -1;
	}
	uint32_t constructor_head;
	uint32_t structural_owner;
	uint32_t constructor_index;
	uint32_t field_terms[64];
	uint32_t field_count;
	if (prototype_term_constructor_spine_info(
			terms, subject, &constructor_head, &structural_owner,
			&constructor_index, field_terms, 64, &field_count
		) != 0 || field_count > expected_domain_capacity) {
		return -1;
	}
	uint32_t owner = constructor_owner_view == PROTOTYPE_INVALID_ID ?
		structural_owner : constructor_owner_view;
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations, terms, owner, &type_id,
			owner_arguments, 64, &owner_argument_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (owner_argument_count != type->parameter_count ||
		constructor_index >= type->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts, substitutions, terms, type_declarations,
			source_context, constructor->parameter_context,
			owner_arguments, owner_argument_count, &substitution
		) != 0) {
		return -1;
	}
	uint32_t declared_field_count;
	if (prototype_context_extension_path(
			contexts, constructor->parameter_context, constructor->field_context,
			(uint32_t[64]) { 0 }, 64, &declared_field_count
		) != 0 || field_count > declared_field_count) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		if (prototype_context_telescope_entry_classifier(
				contexts, substitutions, terms, type_declarations,
				substitution, constructor->parameter_context,
				constructor->field_context, field_terms, i, i,
				&expected_domains[i]
			) != 0) {
			return -1;
		}
	}
	(void)constructor_head;
	*p_domain_count = field_count;
	return 0;
}

int prototype_judgement_apply_indexed_motive(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t motive,
	uint32_t value,
	uint32_t value_classifier,
	int beta_reduce,
	uint32_t* p_result
) {
	if (!terms || !type_declarations || !p_result ||
		motive >= terms->term_count || value >= terms->term_count ||
		value_classifier >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[32];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, value_classifier, &type_id, arguments,
			&argument_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count + type->index_count != argument_count) {
		return -1;
	}
	uint32_t lambda_count = 0;
	uint32_t lambda_cursor = motive;
	while (lambda_cursor < terms->term_count &&
		terms->terms[lambda_cursor].tag == PROTOTYPE_TERM_LAMBDA &&
		lambda_count <= type->index_count) {
		++lambda_count;
		lambda_cursor = terms->terms[lambda_cursor].as.lambda.body;
	}
	uint32_t motive_index_count;
	if (lambda_count == 1) {
		motive_index_count = 0;
	} else if (lambda_count == type->index_count + 1) {
		motive_index_count = type->index_count;
	} else {
		return -1;
	}
	uint32_t current = motive;
	for (uint32_t i = 0; i < motive_index_count; ++i) {
		uint32_t argument = arguments[type->parameter_count + i];
		if (current >= terms->term_count ||
			terms->terms[current].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		if (beta_reduce) {
			if (prototype_term_graph_substitute_bound_var(
					terms, type_declarations,
					terms->terms[current].as.lambda.body,
					terms->terms[current].as.lambda.binding_id,
					argument, &current
				) != 0) {
				return -1;
			}
		} else if (prototype_term_app(
				terms, current, argument, &current
			) != 0) {
			return -1;
		}
	}
	if (beta_reduce) {
		if (current >= terms->term_count ||
			terms->terms[current].tag != PROTOTYPE_TERM_LAMBDA ||
			prototype_term_graph_substitute_bound_var(
				terms, type_declarations,
				terms->terms[current].as.lambda.body,
				terms->terms[current].as.lambda.binding_id,
				value, &current
			) != 0) {
			return -1;
		}
	} else if (prototype_term_app(terms, current, value, &current) != 0) {
		return -1;
	}
	*p_result = current;
	return 0;
}

int prototype_judgement_delta_record_constructor_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_occurrence_ids,
	const struct prototype_judgement_selected_evidence* argument_evidence,
	uint32_t argument_evidence_count
) {
	if (!delta || !terms || !type_declarations || !delta->contexts ||
		!delta->substitutions || argument_evidence_count == 0 ||
		argument_evidence_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		!argument_occurrence_ids || !argument_evidence) {
		return -1;
	}
	struct prototype_judgement_selected_evidence
		current_argument_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < argument_evidence_count; ++i) {
		if (argument_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			argument_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		current_argument_evidence[i] = argument_evidence[i];
		if (argument_evidence[i].context_id != delta->current_context_id) {
			uint32_t projection;
			if (prototype_substitution_projection_path(
					delta->substitutions,
					delta->contexts,
					delta->current_context_id,
					argument_evidence[i].context_id,
					&projection
				) != 0 || prototype_judgement_delta_record_context_weaken(
					delta, &argument_evidence[i], projection
				) != 0 || prototype_judgement_delta_select_evidence(
					delta,
					argument_evidence[i].authority_kind ==
						PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
						argument_evidence[i].occurrence_id : PROTOTYPE_INVALID_ID,
					delta->current_context_id,
					argument_evidence[i].subject,
					argument_evidence[i].classifier,
					&current_argument_evidence[i]
				) != 0) {
				return -1;
			}
		}
		argument_classifiers[i] = current_argument_evidence[i].classifier;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	uint32_t expected_classifier;
	int saturated;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != argument_evidence_count ||
		prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			delta->current_context_id,
			subject,
			owner,
			argument_classifiers,
			argument_evidence_count,
			&expected_classifier,
			&saturated
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms, type_declarations, expected_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)head;
	(void)constructor_index;
	(void)saturated;
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = delta->current_context_id,
		.occurrence_id = delta->current_occurrence_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition
		premise_propositions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	initialize_proof_rule_parameters(
		&proof, proof_premises, premise_propositions
	);
	proof.rule_data.constructor.owner_view = owner;
	proof.premise_count = argument_count;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (current_argument_evidence[i].subject != arguments[i]) {
			return -1;
		}
		proof.premises[i].proposition->kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof.premises[i].proposition->context_id =
			current_argument_evidence[i].context_id;
		proof.premises[i].proposition->occurrence_id = argument_occurrence_ids[i];
		proof.premises[i].proposition->subject = arguments[i];
		proof.premises[i].proposition->classifier =
			current_argument_evidence[i].classifier;
		proof.premises[i].proposition->authority_kind =
			current_argument_evidence[i].authority_kind;
		proof.premises[i].proposition->authority_id =
			current_argument_evidence[i].authority_id;
	}
	return add_complete_delta_relation(delta, &relation, &proof);
}

int prototype_judgement_synthesize_match_pattern_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	struct prototype_judgement_selected_evidence scrutinee_evidence;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&scrutinee_evidence
		) != 0) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	if (prototype_term_type_instance_info(
		terms,
		scrutinee_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(type_declarations, type_id, constructor_symbol_id);
	if (!constructor) {
		return -1;
	}
	return constructor_field_classifier_from_spine(
		delta,
		terms,
		type_declarations,
		scrutinee_classifier,
		constructor->constructor_index,
		NULL,
		0,
		field_index,
		p_classifier
	);
}

int prototype_judgement_resolve_match_case_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_resolution_request* request,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!delta || !terms || !type_declarations || !request || !p_resolution ||
		request->match_term >= terms->term_count ||
		terms->terms[request->match_term].tag != PROTOTYPE_TERM_MATCH ||
		request->case_index >= terms->terms[request->match_term].as.match.case_count) {
		return -1;
	}

	struct prototype_match_constructor_resolution resolution;
	if (prototype_judgement_resolve_match_constructor(
		terms,
		type_declarations,
		delta->contexts,
		request->scrutinee_classifier,
		request->constructor_symbol_id,
		&resolution
	) != 0) {
		return -1;
	}

	uint32_t case_id = terms->terms[request->match_term].as.match.first_case + request->case_index;
	if (case_id >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->binder_count != resolution.field_count) {
		return -1;
	}
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
		struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			if (prototype_term_var(
					terms,
					binder->binding_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					request->scrutinee_classifier,
					resolution.constructor_id,
					&terms->case_binders[match_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_record_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier,
					request->scrutinee_classifier,
					resolution.constructor_id,
					j,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
			if (prototype_judgement_classifier_is_strictly_positive_recursive_field(
					terms,
					type_declarations,
					binder_classifier,
					request->scrutinee_classifier,
					&binder->is_recursive
				) != 0) {
				return -1;
			}
		}

	*p_resolution = resolution;
	return 0;
}

static int match_case_binder_is_recursive_self_field(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	uint32_t binding_id
) {
	if (!delta || !terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}

	uint32_t field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + i];
		if (binder->binding_id == binding_id) {
			field_index = i;
			break;
		}
	}
	if (field_index == PROTOTYPE_INVALID_ID) {
		return 0;
	}

	uint32_t field_classifier;
	if (constructor_field_classifier_from_spine(
			delta,
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id,
			&terms->case_binders[match_case->first_binder],
			field_index,
			field_index,
			&field_classifier
		) != 0) {
		return -1;
	}
	int recursive;
	if (prototype_judgement_classifier_is_strictly_positive_recursive_field(
			terms,
			type_declarations,
			field_classifier,
			match_case->constructor_owner,
			&recursive
		) != 0) {
		return -1;
	}
	return recursive ? 0 : -1;
}

static int induction_hypothesis_classifier_from_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t case_index,
	uint32_t field_index,
	uint32_t* p_classifier,
	uint32_t* p_motive
) {
	if (!delta || !terms || !type_declarations || !p_classifier || !p_motive ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
			argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_classifiers[32];
	uint32_t match_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		if (source == 1 && !delta->db) {
			continue;
		}
		const struct prototype_judgement_proposition* relations =
			source == 0 ? delta->propositions : delta->db->propositions;
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? delta->derivation_candidates :
				delta->db->derivation_candidates;
		size_t proposition_count =
			source == 0 ? delta->proposition_count : delta->db->proposition_count;
		size_t derivation_candidate_count = source == 0 ?
			delta->derivation_candidate_count :
			delta->db->derivation_candidate_count;
		for (size_t i = 0; i < proposition_count; ++i) {
			const struct prototype_judgement_proposition* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != match_term ||
				candidate_has_derivation_other_than(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 1 ||
				!match_motive_result_classifier(terms, match_term, relation->classifier)) {
				continue;
			}
			int contains = classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					match_classifiers,
					match_classifier_count,
					relation->classifier
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				continue;
			}
			if (match_classifier_count >= 32) {
				return -1;
			}
			match_classifiers[match_classifier_count++] = relation->classifier;
		}
	}
	for (size_t i = 0; i < delta->match_motive_result_count; ++i) {
		const struct prototype_judgement_match_motive_result* result =
			&delta->match_motive_results[i];
		if (result->match_term != match_term ||
			!match_motive_result_classifier(terms, match_term, result->classifier)) {
			continue;
		}
		int contains = classifier_list_contains_normalization_equal(
				terms,
				type_declarations,
				match_classifiers,
				match_classifier_count,
				result->classifier
			);
		if (contains < 0) {
			return -1;
		}
		if (contains > 0) {
			continue;
		}
		if (match_classifier_count >= 32) {
			return -1;
		}
		match_classifiers[match_classifier_count++] = result->classifier;
	}
	if (match_classifier_count == 0) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t selected = PROTOTYPE_INVALID_ID;
	uint32_t selected_motive = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_classifier_count; ++i) {
		uint32_t match_classifier = match_classifiers[i];
		if (!term_has_tag(terms, match_classifier, PROTOTYPE_TERM_APP)) {
			continue;
		}
		const struct prototype_term* motive_app = &terms->terms[match_classifier];
		if (motive_app->as.app.argument != match->as.match.scrutinee ||
			!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			continue;
		}
			if (case_index >= match->as.match.case_count) {
				return -1;
			}
			uint32_t case_id = match->as.match.first_case + case_index;
			if (case_id >= terms->case_count ||
				field_index >= terms->cases[case_id].binder_count) {
				return -1;
			}
			const struct prototype_match_case* match_case = &terms->cases[case_id];
			uint32_t field_classifier;
			uint32_t candidate;
			if (constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					match_case->constructor_owner,
					match_case->constructor_id,
					&terms->case_binders[match_case->first_binder],
					field_index,
					field_index,
					&field_classifier
				) != 0 || prototype_judgement_lift_recursive_field_classifier(
					terms,
					type_declarations,
					field_classifier,
					argument,
					match_case->constructor_owner,
					motive_app->as.app.function,
					&candidate
				) != 0) {
				return -1;
			}
		if (selected != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				selected,
				candidate
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		if (selected_motive != PROTOTYPE_INVALID_ID &&
			selected_motive != motive_app->as.app.function) {
			return -1;
		}
		selected = candidate;
		selected_motive = motive_app->as.app.function;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	*p_motive = selected_motive;
	(void)delta;
	return 0;
}

static int induction_hypothesis_context_from_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t* p_context_case_index,
	uint32_t* p_context_field_index
) {
	if (!delta || !terms || !type_declarations || !p_context_case_index ||
		!p_context_field_index ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
		argument >= terms->term_count ||
		terms->terms[argument].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binding_id = terms->terms[argument].as.var.binding_id;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binding_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive > 0) {
			continue;
		}
		for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
			if (terms->case_binders[terms->cases[case_id].first_binder + j].binding_id ==
				binding_id) {
				*p_context_case_index = i;
				*p_context_field_index = j;
				return 0;
			}
		}
	}
	return 1;
}

static int collect_judgement_subject_classifiers(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
);
struct app_classifier_candidate {
	struct prototype_judgement_selected_evidence function_evidence;
	struct prototype_judgement_selected_evidence argument_evidence;
	uint32_t function_pi;
	uint32_t result_classifier;
};

static int collect_app_classifier_candidates_from_candidates(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t argument,
	const struct prototype_judgement_selected_evidence* function_candidates,
	uint32_t function_candidate_count,
	const struct prototype_judgement_selected_evidence* argument_candidates,
	uint32_t argument_candidate_count,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!function_candidates || !argument_candidates ||
		!candidates || !p_candidate_count ||
		argument >= terms->term_count) {
		return -1;
	}
	*p_candidate_count = 0;
	for (uint32_t i = 0; i < function_candidate_count; ++i) {
		uint32_t function_pi;
		uint32_t domain;
		uint32_t codomain_family;
		int status = classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			function_candidates[i].classifier,
			&function_pi,
			&domain,
			&codomain_family
		);
		(void)codomain_family;
		if (status < 0) {
			return -1;
		}
		if (status > 0) {
			continue;
		}
		for (uint32_t j = 0; j < argument_candidate_count; ++j) {
			uint32_t result_classifier;
			if (!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					domain,
					argument_candidates[j].classifier
				)) {
				continue;
			}
			if (pi_codomain_after_argument_in_context(
					contexts,
					substitutions,
					terms,
					type_declarations,
					context_id,
					function_pi,
					argument,
					argument_candidates[j].classifier,
					&result_classifier
				) != 0) {
				return -1;
			}
			int duplicate = 0;
			for (uint32_t k = 0; k < *p_candidate_count; ++k) {
				int result_equal;
				if (classifier_conversion_decision(
						prototype_judgement_classifier_conversion(
							terms,
							type_declarations,
							candidates[k].result_classifier,
							result_classifier
						),
						&result_equal
					) != 0) {
					return -1;
				}
				if (selected_evidence_equal(
						&candidates[k].function_evidence,
						&function_candidates[i]
					) && selected_evidence_equal(
						&candidates[k].argument_evidence,
						&argument_candidates[j]
					) && result_equal) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			if (*p_candidate_count >= candidate_capacity) {
				return -1;
			}
			candidates[*p_candidate_count].function_evidence = function_candidates[i];
			candidates[*p_candidate_count].argument_evidence = argument_candidates[j];
			candidates[*p_candidate_count].function_pi = function_pi;
			candidates[*p_candidate_count].result_classifier = result_classifier;
			(*p_candidate_count)++;
		}
	}
	return 0;
}

static int collect_delta_app_classifier_candidates(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function,
	uint32_t argument,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!delta || !terms || !type_declarations ||
		function >= terms->term_count ||
		argument >= terms->term_count) {
		return -1;
	}
	struct prototype_judgement_selected_evidence function_candidates[32];
	struct prototype_judgement_selected_evidence argument_candidates[32];
	uint32_t function_candidate_count = 0;
	uint32_t argument_candidate_count = 0;
	if (collect_subject_evidence(
			delta,
			function,
			function_candidates,
			32,
			&function_candidate_count
		) != 0 ||
		collect_subject_evidence(
			delta,
			argument,
			argument_candidates,
			32,
			&argument_candidate_count
		) != 0) {
		return -1;
	}
	if (function_candidate_count == 0 || argument_candidate_count == 0) {
		return 1;
	}
	if (collect_app_classifier_candidates_from_candidates(
			delta->contexts,
			delta->substitutions,
			terms,
			type_declarations,
			delta->current_context_id,
			argument,
		function_candidates,
			function_candidate_count,
			argument_candidates,
			argument_candidate_count,
			candidates,
			candidate_capacity,
			p_candidate_count
		) != 0) {
		return -1;
	}
	return *p_candidate_count == 0 ? 1 : 0;
}

static int prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[subject];
	if (ih->as.induction_hypothesis.ih_scope_id >= terms->ih_scope_count ||
		ih->as.induction_hypothesis.argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_term =
		terms->ih_scopes[ih->as.induction_hypothesis.ih_scope_id].match_term;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	int context_status = induction_hypothesis_context_from_argument(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		&context_case_index,
		&context_field_index
	);
	if (context_status != 0) {
		return context_status < 0 ? -1 : 1;
	}
	uint32_t classifier;
	uint32_t motive;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		context_case_index,
		context_field_index,
		&classifier,
		&motive
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		(void)argument_classifier;
		return 1;
	}
	return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		subject,
		classifier,
		match_term,
		motive,
		context_case_index,
		context_field_index,
		NULL,
		0
	);
}

int prototype_judgement_delta_resolve_induction_hypothesis_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_induction_hypothesis_resolution_request* request
) {
	if (!delta || !terms || !type_declarations || !request ||
		!term_has_tag(terms, request->subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		request->argument >= terms->term_count ||
		terms->terms[request->argument].tag != PROTOTYPE_TERM_VAR ||
		request->ih_scope_id >= terms->ih_scope_count) {
		return -1;
	}
	const struct prototype_term* subject = &terms->terms[request->subject];
	if (subject->as.induction_hypothesis.ih_scope_id != request->ih_scope_id ||
		subject->as.induction_hypothesis.argument != request->argument) {
		return -1;
	}
	uint32_t ignored_existing_classifier;
	int has_existing_classifier =
		lookup_delta_proven_classifier(
			delta,
			terms,
			request->subject,
			&ignored_existing_classifier
		) == 0;

	uint32_t match_term = terms->ih_scopes[request->ih_scope_id].match_term;
	if (!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH)) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binding_id = terms->terms[request->argument].as.var.binding_id;
	int found = 0;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binding_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive == 1) {
			return 1;
		}
		if (recursive == 0) {
			for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
				if (terms->case_binders[terms->cases[case_id].first_binder + j].binding_id ==
					binding_id) {
					found = 1;
					context_case_index = i;
					context_field_index = j;
					break;
				}
			}
		}
	}
	if (!found) {
		return -1;
	}

	uint32_t classifier;
	uint32_t motive;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		request->argument,
		context_case_index,
		context_field_index,
		&classifier,
		&motive
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		if (has_existing_classifier) {
			return 0;
		}
		return 1;
	}
	if (status != 0) {
		return status;
	}
		struct prototype_judgement_selected_evidence existing_evidence;
		if (has_existing_classifier &&
			lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				request->subject,
				classifier,
				&existing_evidence
			) == 0) {
			return 0;
		}
		(void)has_existing_classifier;
		return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		request->subject,
		classifier,
		match_term,
			motive,
			context_case_index,
			context_field_index,
			NULL,
			0
		);
}

static int type_instance_has_known_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	return prototype_term_type_instance_is_saturated(terms, type_declarations, term_id);
}

static int term_is_structural_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
		return term_is_universe_var(terms, term_id) ||
			term_is_primitive_text(terms, term_id) ||
			term_is_primitive_int(terms, term_id) ||
			term_is_primitive_int64(terms, term_id) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_EXTERNAL_REF) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_PI) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_COMPUTATION_TYPE) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_THUNK_TYPE) ||
		type_instance_has_known_type(terms, type_declarations, term_id);
}

static int infer_type_formation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !delta->db || !terms || !type_declarations ||
		!type_instance_has_known_type(terms, type_declarations, term_id)) {
		return -1;
	}
	uint32_t existing_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, term_id, &existing_classifier
		) == 0 && term_is_universe_var(terms, existing_classifier)) {
		return 0;
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			terms,
			delta->db->next_universe_var++,
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

static int classifier_returns_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner
) {
	uint32_t owner_type_id;
	uint32_t owner_arguments[16];
	uint32_t owner_argument_count;
	int owner_is_type_instance = prototype_term_type_instance_info(
		terms,
		owner,
		&owner_type_id,
		owner_arguments,
		&owner_argument_count
	) == 0 && owner_type_id < type_declarations->type_count;
	if (!owner_is_type_instance && owner < terms->term_count) {
		uint32_t reversed[16];
		uint32_t current_owner = owner;
		owner_argument_count = 0;
		while (current_owner < terms->term_count &&
			terms->terms[current_owner].tag == PROTOTYPE_TERM_APP) {
			if (owner_argument_count >= 16) {
				return 0;
			}
			reversed[owner_argument_count++] =
				terms->terms[current_owner].as.app.argument;
			current_owner = terms->terms[current_owner].as.app.function;
		}
		if (current_owner < terms->term_count &&
			terms->terms[current_owner].tag == PROTOTYPE_TERM_TYPE_FORMER &&
			prototype_type_declaration_representation_type_id(
				type_declarations,
				terms->terms[current_owner].as.type_former.representation_id,
				&owner_type_id
			) == 0) {
			for (uint32_t i = 0; i < owner_argument_count; ++i) {
				owner_arguments[i] = reversed[owner_argument_count - i - 1];
			}
			owner_is_type_instance = 1;
		}
	}
	uint32_t current = classifier;
	while (current < terms->term_count) {
		int same_owner = 0;
		if (current == owner) {
			return 1;
		}
		/* Constructor owners are erased to the computation-level family before
		 * accepted replay, while their classifiers retain the nominal TypeView.
		 * The view's core is therefore the authoritative counterpart of owner. */
		if (terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
			uint32_t view_core = terms->terms[current].as.type_view.core;
			if (prototype_term_core_shape_equal(
					terms, view_core, owner, &same_owner
				) != 0) {
				return 0;
			}
			if (same_owner) {
				return 1;
			}
		}
		if (prototype_term_core_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		if (prototype_term_source_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		/* An indexed constructor may retain its parameter-only owner view or a
		 * fully applied source view. Both select the declaration through uniform
		 * parameters. Result indices are established only after field application
		 * and are checked by the constructor-spine rule. */
		if (owner_is_type_instance) {
			uint32_t result_type_id;
			uint32_t result_arguments[16];
			uint32_t result_argument_count;
			const struct prototype_type_declaration* declaration =
				&type_declarations->type_declarations[owner_type_id];
			int result_instance = prototype_term_type_instance_info(
					terms,
					current,
					&result_type_id,
					result_arguments,
					&result_argument_count
				) == 0;
			if (result_instance && result_type_id == owner_type_id &&
				(owner_argument_count == declaration->parameter_count ||
				 owner_argument_count == declaration->parameter_count +
					declaration->index_count) &&
				result_argument_count == declaration->parameter_count +
					declaration->index_count) {
				int parameters_match = 1;
				for (uint32_t i = 0; i < declaration->parameter_count; ++i) {
					int equal = 0;
					if (prototype_term_core_shape_equal(
							terms,
							owner_arguments[i],
							result_arguments[i],
							&equal
						) != 0) {
						parameters_match = 0;
						break;
					}
					if (!equal && prototype_term_source_shape_equal(
							terms,
							owner_arguments[i],
							result_arguments[i],
							&equal
						) != 0) {
						parameters_match = 0;
						break;
					}
					if (!equal) {
						parameters_match = 0;
						break;
					}
				}
				if (parameters_match) {
					return 1;
				}
			}
		}
		uint32_t domain;
		uint32_t codomain_lambda;
		if (pi_parts(terms, current, &domain, &codomain_lambda) != 0) {
			return 0;
		}
		(void)domain;
		(void)codomain_lambda;
		uint32_t ignored_binder;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[current].as.pi.codomain_family,
				&ignored_binder,
				&current
			) != 0) {
			return 0;
		}
	}
	return 0;
}

static int constructor_belongs_to_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	if (!terms || !type_declarations || owner >= terms->term_count) {
		return 0;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms, owner, &type_id, args, &arg_count
		) != 0) {
		uint32_t reversed[16];
		uint32_t count = 0;
		uint32_t current = owner;
		while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_APP) {
			if (count >= 16) {
				return 0;
			}
			reversed[count++] = terms->terms[current].as.app.argument;
			current = terms->terms[current].as.app.function;
		}
		if (current >= terms->term_count ||
			terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER ||
			prototype_type_declaration_representation_type_id(
				type_declarations,
				terms->terms[current].as.type_former.representation_id,
				&type_id
			) != 0) {
			return 0;
		}
		for (uint32_t i = 0; i < count; ++i) {
			args[i] = reversed[count - i - 1];
		}
		arg_count = count;
	}
	if (type_id >= type_declarations->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count > UINT32_MAX - type->index_count ||
		(arg_count != type->parameter_count &&
		 arg_count != type->parameter_count + type->index_count)) {
		return 0;
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			/* A constructor head is owned by the family after uniform parameters.
			 * Its fields subsequently determine the exact result indices. */
			if (arg_count == type->parameter_count) {
				return 1;
			}
			/* A fully indexed owner is valid only when it is the exact constructor
			 * result without unspecialized fields. */
			int same_result = 0;
			return constructor->field_context == constructor->parameter_context &&
				prototype_term_source_shape_equal(
					terms,
					constructor->result_classifier,
					owner,
					&same_result
				) == 0 && same_result;
		}
	}
	return 0;
}

static int match_case_has_valid_constructor(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case
) {
	if (!terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID ||
		!term_exists(terms, match_case->constructor_owner)) {
		return 0;
	}
	if (constructor_belongs_to_owner(
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id
		)) {
		return 1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms,
			match_case->constructor_owner,
			&type_id,
			args,
			&arg_count
		) == 0 &&
		type_id < type_declarations->type_count) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_id];
		for (uint32_t i = 0; i < type->constructor_count; ++i) {
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[
					type->first_constructor + i
				];
			if (constructor->owner_type == type_id &&
				constructor->constructor_index == match_case->constructor_id) {
				return 1;
			}
		}
		return 0;
	}
	(void)type_id;
	(void)args;
	(void)arg_count;
	return 1;
}

static int type_formation_is_nat_shape(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || !contexts ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count != 0 || type->constructor_count != 2) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* zero = NULL;
	const struct prototype_type_constructor_declaration* succ = NULL;
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == 0) {
			zero = constructor;
		} else if (constructor->constructor_index == 1) {
			succ = constructor;
		}
	}
	if (!zero || !succ ||
		zero->owner_type != type_id ||
		succ->owner_type != type_id) {
		return 0;
	}

	uint32_t zero_fields[1];
	uint32_t zero_field_count;
	uint32_t succ_fields[1];
	uint32_t succ_field_count;
	if (prototype_context_extension_path(
			contexts,
			zero->parameter_context,
			zero->field_context,
			zero_fields,
			1,
			&zero_field_count
		) != 0 ||
		prototype_context_extension_path(
			contexts,
			succ->parameter_context,
			succ->field_context,
			succ_fields,
			1,
			&succ_field_count
		) != 0) {
		return 0;
	}
	if (zero_field_count != 0 || succ_field_count != 1) {
		return 0;
	}
	const struct prototype_context* succ_field =
		prototype_context_get(contexts, succ_fields[0]);
	uint32_t succ_classifier = prototype_context_classifier_term(succ_field);
	if (!succ_field || succ_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (!(prototype_judgement_classifier_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)type_declarations,
			succ_classifier,
			term_id
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	return 1;
}

static int type_formation_has_name_symbol(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	int name_symbol_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || name_symbol_id < 0 ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	return type_declarations->type_declarations[type_id].name_symbol_id == name_symbol_id;
}

static void candidate_claim_authority(
	const struct prototype_judgement_proposition* relation,
	int* p_authority_kind,
	uint32_t* p_authority_id
);
