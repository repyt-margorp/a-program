static int operation_named_nat_type_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	int nat_symbol_id,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret || nat_symbol_id < 0) {
		return -1;
	}
	const struct prototype_type_declaration* nat_type =
		prototype_type_declaration_lookup(type_declarations, nat_symbol_id);
	if (!nat_type) {
		return -1;
	}
	return prototype_term_type_instance_make(
		terms,
		type_declarations,
		nat_type->type_index,
		NULL,
		0,
		p_ret
	);
}

static int host_signature_classifier(
	struct prototype_term_db* terms,
	uint32_t arity,
	const int* argument_types,
	int result_type,
	uint32_t* p_ret
) {
	if (!terms || !argument_types || !p_ret ||
		result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return 1;
	}
	for (uint32_t i = 0; i < arity; ++i) {
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
			return 1;
		}
	}
	uint32_t current;
	if (prototype_term_make_host_type(terms, result_type, &current) != 0) {
		return -1;
	}
	uint32_t effect_row;
	if (prototype_term_effect_row_empty(terms, &effect_row) != 0 ||
		prototype_term_computation_type(terms, effect_row, current, &current) != 0) {
		return -1;
	}
	for (uint32_t i = arity; i > 0; --i) {
		int argument_type = argument_types[i - 1];
		uint32_t domain;
		if (prototype_term_make_host_type(terms, argument_type, &domain) != 0 ||
			prototype_term_pi(terms, domain, current, &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

int prototype_judgement_pure_primitive_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_PURE_PRIMITIVE) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(operation->as.pure_primitive.primitive_id);
	if (signature) {
		int status = host_signature_classifier(
			terms,
			signature->arity,
			signature->argument_types,
			signature->result_type,
			p_ret
		);
		if (status <= 0) {
			return status;
		}
	}

	uint32_t text;
	uint32_t nat;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	switch (operation->as.pure_primitive.primitive_id) {
		case PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_row_empty(terms, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, nat, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, text, result, p_ret);
		}
		case PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_row_empty(terms, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, text, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, nat, result, p_ret);
		}
		default:
			return -1;
	}
	}

int prototype_judgement_effect_operation_classifier(
	struct prototype_term_db* terms,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	*p_ret = operation->as.effect_operation.classifier;
	return 0;
}

static int infer_text_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms || !term_has_tag(terms, term_id, PROTOTYPE_TERM_TEXT_LITERAL)) {
		return -1;
	}
	uint32_t text;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		text,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

static int infer_int_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !delta->intrinsic_environment || !terms ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_INT_LITERAL)) {
		return -1;
	}
	uint32_t integer;
	int integer_type = delta->intrinsic_environment->default_integer_host_type;
	int bit_width = prototype_term_host_type_bit_width(integer_type);
	int64_t value = terms->terms[term_id].as.int_literal.value;
	if ((bit_width == 32 && (value < INT32_MIN || value > INT32_MAX)) ||
		(bit_width != 32 && bit_width != 64) ||
		prototype_term_make_host_type(terms, integer_type, &integer) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		integer,
			PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
		);
}

static int infer_pure_primitive_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_PURE_PRIMITIVE)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[term_id],
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

static int infer_effect_operation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_EFFECT_OPERATION)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[term_id], &classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

static int infer_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_CONSTRUCTOR)) {
		return -1;
	}
	uint32_t constructor_term;
	uint32_t classifier;
	const struct prototype_term* term = &terms->terms[term_id];
	return materialize_constructor_classifier(
		delta,
		terms,
		type_declarations,
		term->as.constructor.owner,
		term->as.constructor.constructor_id,
		&constructor_term,
		&classifier
	);
}

int prototype_judgement_cbpv_boundary_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_classifier,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !p_classifier || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t classifier;
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		uint32_t empty_effects;
		if (prototype_term_effect_row_empty(terms, &empty_effects) != 0 ||
			prototype_term_computation_type(terms, empty_effects, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, child_classifier, &view
			) != 0 ||
			view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			prototype_term_thunk_type(terms, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		uint32_t value_whnf;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, child_classifier, &value_whnf
			) != 0 || value_whnf >= terms->term_count ||
			terms->terms[value_whnf].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		classifier = terms->terms[value_whnf].as.thunk_type.computation;
	} else {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

static int record_cbpv_boundary_with_evidence(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_operation_id,
	uint32_t child_classifier,
	const struct prototype_judgement_selected_evidence* child_evidence
) {
	if (!delta || !terms || !type_declarations || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t child;
	int proof_kind;
	uint32_t classifier;
	if (prototype_judgement_cbpv_boundary_classifier(
			terms, type_declarations, term_id, child_classifier, &classifier
		) != 0) {
		return -1;
	}
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		child = term->as.return_term.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		child = term->as.thunk.computation;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		child = term->as.force.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { child };
	uint32_t premise_classifiers[1] = { child_classifier };
	uint32_t premise_occurrence_ids[1] = { child_operation_id };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		proof_kind,
		NULL,
		premise_occurrence_ids,
		premise_subjects,
		premise_classifiers,
		child_evidence,
		1
	);
}

int prototype_judgement_delta_record_cbpv_boundary(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_operation_id,
	const struct prototype_judgement_selected_evidence* child_evidence
) {
	if (!child_evidence ||
		child_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		child_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return -1;
	}
	return record_cbpv_boundary_with_evidence(
		delta,
		terms,
		type_declarations,
		term_id,
		child_operation_id,
		child_evidence->classifier,
		child_evidence
	);
}

static int infer_cbpv_boundary_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t child;
	switch (terms->terms[term_id].tag) {
		case PROTOTYPE_TERM_RETURN:
			child = terms->terms[term_id].as.return_term.value;
			break;
		case PROTOTYPE_TERM_THUNK:
			child = terms->terms[term_id].as.thunk.computation;
			break;
		case PROTOTYPE_TERM_FORCE:
			child = terms->terms[term_id].as.force.value;
			break;
		default:
			return -1;
	}
	struct prototype_judgement_selected_evidence child_evidence;
	int selection_status = lookup_delta_selected_evidence(
		delta, child, 1, &child_evidence
	);
	if (selection_status != 0) {
		return 1;
	}
	return record_cbpv_boundary_with_evidence(
		delta,
		terms,
		type_declarations,
		term_id,
		child_evidence.occurrence_id,
		child_evidence.classifier,
		&child_evidence
	);
}

int prototype_judgement_delta_infer_cbpv_boundaries(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			int tag = terms->terms[i].tag;
			if (tag != PROTOTYPE_TERM_RETURN &&
				tag != PROTOTYPE_TERM_THUNK &&
				tag != PROTOTYPE_TERM_FORCE) {
				continue;
			}
			uint32_t ignored;
			if (lookup_delta_proven_classifier(delta, terms, i, &ignored) == 0) {
				continue;
			}
			size_t before = delta->proposition_count;
			int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
			if (status < 0) {
				return -1;
			}
			if (delta->proposition_count > before) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	return 0;
}

int prototype_judgement_delta_record_computation_constraint(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject
) {
	if (!delta || !terms ||
		(delta->computation_constraint_capacity != 0 &&
			!delta->computation_constraints) ||
		subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[subject];
	struct prototype_judgement_computation_constraint constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.context_id = context_id;
	constraint.occurrence_id = delta->current_occurrence_id;
	constraint.subject = subject;
	if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		constraint.kind = PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD;
		constraint.computation = term->as.computation_fold.computation;
		constraint.continuation = term->as.computation_fold.return_clause;
		constraint.argument = PROTOTYPE_INVALID_ID;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else if (term->tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
		constraint.kind =
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		constraint.computation = term->as.operation_request.operation;
		constraint.continuation = term->as.operation_request.continuation;
		constraint.argument = term->as.operation_request.argument;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else {
		return 1;
	}
	if (constraint.occurrence_id != PROTOTYPE_INVALID_ID) {
		if (constraint.occurrence_id >=
			PROTOTYPE_JUDGEMENT_OCCURRENCE_INDEX_CAPACITY) {
			return -1;
		}
		uint32_t existing_id = delta->computation_constraint_by_occurrence[
			constraint.occurrence_id
		];
		if (existing_id != PROTOTYPE_INVALID_ID) {
			if (existing_id >= delta->computation_constraint_count) {
				return -1;
			}
			const struct prototype_judgement_computation_constraint* existing =
				&delta->computation_constraints[existing_id];
			return existing->kind == constraint.kind &&
				existing->context_id == context_id &&
				existing->occurrence_id == constraint.occurrence_id &&
				existing->subject == subject ? 0 : -1;
		}
	} else {
		for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
			const struct prototype_judgement_computation_constraint* existing =
				&delta->computation_constraints[i];
			if (existing->kind == constraint.kind &&
				existing->context_id == context_id &&
				existing->occurrence_id == PROTOTYPE_INVALID_ID &&
				existing->subject == subject) {
				return 0;
			}
		}
	}
	if (reserve_slot(
			delta->computation_constraint_count,
			delta->computation_constraint_capacity
		) != 0) {
		return -1;
	}
	uint32_t constraint_id = (uint32_t)delta->computation_constraint_count;
	delta->computation_constraints[delta->computation_constraint_count++] = constraint;
	if (constraint.occurrence_id != PROTOTYPE_INVALID_ID) {
		delta->computation_constraint_by_occurrence[constraint.occurrence_id] =
			constraint_id;
	}
	return 0;
}

int prototype_judgement_delta_rebuild_computation_constraint_index(
	struct prototype_judgement_delta* delta
) {
	if (!delta || (delta->computation_constraint_count != 0 &&
		!delta->computation_constraints)) {
		return -1;
	}
	for (uint32_t i = 0;
		i < PROTOTYPE_JUDGEMENT_OCCURRENCE_INDEX_CAPACITY;
		++i) {
		delta->computation_constraint_by_occurrence[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < delta->computation_constraint_count; ++i) {
		uint32_t occurrence = delta->computation_constraints[i].occurrence_id;
		if (occurrence == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (occurrence >= PROTOTYPE_JUDGEMENT_OCCURRENCE_INDEX_CAPACITY ||
			delta->computation_constraint_by_occurrence[occurrence] !=
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		delta->computation_constraint_by_occurrence[occurrence] = i;
	}
	return 0;
}

int prototype_judgement_delta_computation_constraint_for_occurrence(
	const struct prototype_judgement_delta* delta,
	uint32_t occurrence_id,
	uint32_t* p_constraint_id
) {
	if (!delta || !p_constraint_id || occurrence_id >=
		PROTOTYPE_JUDGEMENT_OCCURRENCE_INDEX_CAPACITY) {
		return -1;
	}
	uint32_t id = delta->computation_constraint_by_occurrence[occurrence_id];
	if (id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (id >= delta->computation_constraint_count ||
		delta->computation_constraints[id].occurrence_id != occurrence_id) {
		return -1;
	}
	*p_constraint_id = id;
	return 0;
}

int prototype_judgement_delta_generate_computation_constraints(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms
) {
	if (!delta || !terms) {
		return -1;
	}
	uint32_t saved_operation_id = delta->current_occurrence_id;
	delta->current_occurrence_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		int status = prototype_judgement_delta_record_computation_constraint(
			delta, terms, delta->current_context_id, i
		);
		if (status < 0) {
			delta->current_occurrence_id = saved_operation_id;
			return -1;
		}
	}
	delta->current_occurrence_id = saved_operation_id;
	return 0;
}

/* A zero-clause computation fold sequences a computation through its return
 * algebra. A function computation produces its suspended function value, as
 * prescribed by the matching runtime reduction for a LAMBDA input. */
int prototype_judgement_computation_fold_result_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_computation,
	uint32_t input_classifier,
	uint32_t continuation_classifier,
	uint32_t* p_ret
) {
	struct prototype_term_classifier_view input_view;
	struct prototype_term_classifier_view continuation_view;
	uint32_t continuation_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	uint32_t dependency_codomain;
	uint32_t normalized_dependency_codomain;
	uint32_t normalized_input;
	struct prototype_term_normalization_result input_normalization;
	uint32_t codomain_lambda;
	uint32_t applied_codomain;
	uint32_t normalized_codomain;
	uint32_t input_value_classifier;
	uint32_t input_effect_row;
	if (!terms || !type_declarations || !p_ret ||
		input_computation >= terms->term_count || input_classifier >= terms->term_count ||
		continuation_classifier >= terms->term_count) {
		return -1;
	}
	if (terms->terms[input_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		/* An IH occurrence is lowered before its motive fixes polarity. If the
		 * solved motive is a suspended function value, the provisional fold is
		 * the implicit RETURN boundary for that value. */
		input_value_classifier = input_classifier;
		if (prototype_term_effect_row_empty(terms, &input_effect_row) != 0) {
			return -1;
		}
	} else {
		int input_view_status = prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		);
		if (input_view_status != 0 ||
			input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			(input_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
			 input_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION)) {
			return -1;
		}
		if (input_view.computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			input_value_classifier = input_view.result;
			input_effect_row = input_view.effect_row;
		} else {
			uint32_t callable_classifier;
			if (prototype_judgement_expose_callable_classifier(
					terms,
					type_declarations,
					input_classifier,
					&callable_classifier
				) != 0 || prototype_term_thunk_type(
					terms, callable_classifier, &input_value_classifier
				) != 0 || prototype_term_effect_row_empty(
					terms, &input_effect_row
				) != 0) {
				return -1;
			}
		}
	}
	int continuation_pi_status = classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			continuation_classifier,
			&continuation_pi,
			&domain,
			&codomain_family
		);
	int family_status = continuation_pi_status == 0 ? prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) : -1;
	int domain_conversion = continuation_pi_status == 0 ?
		prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, input_value_classifier
		).status : PROTOTYPE_TERM_CONVERSION_INVALID;
	if (continuation_pi_status != 0 || family_status != 0 ||
		domain_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	dependency_codomain = codomain;
	if (terms->terms[dependency_codomain].tag == PROTOTYPE_TERM_RETURN) {
		dependency_codomain =
			terms->terms[dependency_codomain].as.return_term.value;
	}
	/* Dependency is semantic, not a property of an unreduced classifier
	 * expression. In particular, APP(\_. C, x) must not force a neutral input
	 * computation merely because x occurs before beta reduction. */
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			dependency_codomain,
			&normalized_dependency_codomain
		) != 0) {
		return -1;
	}
	if (!prototype_term_contains_free_binding(
			terms, normalized_dependency_codomain, codomain_binder
		)) {
		applied_codomain = normalized_dependency_codomain;
	} else {
		if (prototype_term_normalize_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				input_computation,
				PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
				&input_normalization
			) != 0 || input_normalization.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (input_normalization.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			return 1;
		}
		normalized_input = input_normalization.term_id;
		uint32_t input_value;
		if (normalized_input >= terms->term_count) {
			return -1;
		}
		if (terms->terms[normalized_input].tag == PROTOTYPE_TERM_RETURN) {
			input_value = terms->terms[normalized_input].as.return_term.value;
		} else if (terms->terms[normalized_input].tag == PROTOTYPE_TERM_THUNK) {
			input_value = normalized_input;
		} else if (terms->terms[normalized_input].tag == PROTOTYPE_TERM_LAMBDA) {
			if (prototype_term_thunk(terms, normalized_input, &input_value) != 0) {
				return -1;
			}
		} else {
			int projection_status = prototype_term_project_pure_computation_value(
				terms,
				type_declarations,
				input_computation,
				PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
				&input_value
			);
			if (projection_status != 0) {
				/* COMPLETE means that a WHNF was reached, not that the computation
				 * returned. Keep an opaque pure result as a residual dependent
				 * sequencing equation. */
				return projection_status < 0 ? -1 : 1;
			}
		}
		if (
			prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				&codomain_lambda
			) != 0 || prototype_term_app(
				terms,
				codomain_lambda,
				input_value,
				&applied_codomain
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			applied_codomain,
			&normalized_codomain
		) != 0 || normalized_codomain >= terms->term_count) {
		return -1;
	}
	if (terms->terms[normalized_codomain].tag == PROTOTYPE_TERM_RETURN) {
		applied_codomain = terms->terms[normalized_codomain].as.return_term.value;
	} else {
		applied_codomain = normalized_codomain;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, applied_codomain, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t effect_row;
	if (prototype_term_effect_row_union(
			terms, input_effect_row, continuation_view.effect_row, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, p_ret
		) != 0) {
		return -1;
	}
	(void)continuation_pi;
	(void)codomain_binder;
	(void)codomain;
	return 0;
}

int prototype_judgement_delta_record_computation_fold_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_occurrence_ids,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	if (!delta || !terms || !type_declarations ||
		subject >= terms->term_count || classifier >= terms->term_count ||
		terms->terms[subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		!premise_occurrence_ids || !premise_evidence) {
		return -1;
	}
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (premise_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		premise_classifiers[i] = premise_evidence[i].classifier;
	}
	const struct prototype_term* fold = &terms->terms[subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	uint32_t expected_premise_count = 2 + 2 * clause_count;
	if (clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		premise_count != expected_premise_count ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (premise_classifiers[i] >= terms->term_count) {
			return -1;
		}
	}
	if (clause_count == 0) {
		uint32_t derived_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			premise_classifiers[0],
			premise_classifiers[1],
			&derived_classifier
		);
		if (status != 0) {
			return status;
		}
		if (!(prototype_judgement_classifier_conversion(
				terms, type_declarations, classifier, derived_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			/* The Operation classifier may still be a fixed-point approximation.
			 * A mismatch is a deferred equation; final publication separately
			 * requires an exact grounded Claim or residual obligation. */
			return 1;
		}
	} else {
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, classifier, &result
			) != 0 || result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return -1;
		}
	}
	uint32_t subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
	}
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL,
		premise_occurrence_ids,
		subjects,
		premise_classifiers,
		premise_evidence,
		premise_count
	);
}

static int select_closed_constraint_premises(
	const struct prototype_judgement_computation_constraint* constraint,
	const uint32_t* subjects,
	const uint32_t* classifiers,
	uint32_t premise_count,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!constraint || !subjects || !classifiers || !selected ||
		constraint->occurrence_id == PROTOTYPE_INVALID_ID ||
		premise_count > constraint->premise_occurrence_count) {
		return -1;
	}
	memset(
		selected,
		0,
		premise_count * sizeof(struct prototype_judgement_selected_evidence)
	);
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (constraint->premise_states[i] ==
			PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL) {
			selected[i] = (struct prototype_judgement_selected_evidence){
				.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				.authority_id = subjects[i],
				.context_id = constraint->premise_contexts[i],
				.occurrence_id = PROTOTYPE_INVALID_ID,
				.subject = subjects[i],
				.classifier = classifiers[i]
			};
			continue;
		}
		if (constraint->premise_states[i] ==
			PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
			return 1;
		}
		if (constraint->premise_states[i] !=
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED ||
			constraint->premise_occurrences[i] == PROTOTYPE_INVALID_ID ||
			classifiers[i] == PROTOTYPE_INVALID_ID ||
			constraint->premise_evidence[i].kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			constraint->premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			constraint->premise_evidence[i].subject != subjects[i] ||
			constraint->premise_evidence[i].classifier != classifiers[i]) {
			return -1;
		}
		selected[i] = constraint->premise_evidence[i];
	}
	return 0;
}

static int solve_zero_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint,
	struct prototype_judgement_computation_constraint_result* result
) {
	uint32_t input_classifier;
	struct prototype_judgement_selected_evidence input_evidence;
	struct prototype_term_classifier_view input_view;
	int input_status;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_occurrence_count < 2 ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		input_classifier = constraint->premise_classifiers[0];
		input_status = 0;
	} else {
		if (term_has_tag(
				terms,
				constraint->computation,
				PROTOTYPE_TERM_OPERATION_REQUEST
			)) {
			input_status = lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->computation,
				PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
				&input_evidence
			);
		} else if (term_has_tag(
				terms,
				constraint->computation,
				PROTOTYPE_TERM_COMPUTATION_FOLD
			)) {
			input_status = lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->computation,
				PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
				&input_evidence
			);
		} else {
			input_status = lookup_delta_selected_evidence(
				delta, constraint->computation, 1, &input_evidence
			);
		}
		if (input_status == 0) {
			input_classifier = input_evidence.classifier;
		}
	}
	if (input_status != 0) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		/* TermDB identity may be shared by differently typed source occurrences.
		 * A global lookup that finds a non-returning candidate cannot refute this
		 * occurrence-local zero-clause computation fold; defer until the TypedOccurrenceGraph records its selected
		 * operand classifiers. */
		return 1;
	}
	if (constraint->occurrence_id == PROTOTYPE_INVALID_ID &&
		term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_LAMBDA) &&
		infer_lambda_classifier_for_app_argument(
			delta,
			terms,
			type_declarations,
			constraint->continuation,
			input_view.result
		) != 0) {
		return 1;
	}
	uint32_t continuation_classifier;
	struct prototype_judgement_selected_evidence continuation_evidence;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		continuation_classifier = constraint->premise_classifiers[1];
		if (continuation_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
	} else {
		if (lookup_delta_selected_evidence(
				delta, constraint->continuation, 1, &continuation_evidence
			) != 0) {
			return 1;
		}
		continuation_classifier = continuation_evidence.classifier;
	}
	uint32_t classifier;
	int result_status = prototype_judgement_computation_fold_result_classifier(
		terms,
		type_declarations,
		constraint->computation,
		input_classifier,
		continuation_classifier,
		&classifier
	);
	if (result_status != 0) {
		/* This legacy constraint stores only TermDB subjects.  A shared core
		 * term may have several occurrence-local classifiers, so failure for the
		 * candidates selected above cannot refute the source computation fold.  Derive a
		 * proof here only when the candidates work; otherwise let the
		 * TypedOccurrenceGraph validate its explicitly selected child classifiers. */
		return 1;
	}
	result->solved_classifier = classifier;
	if (result->projected_classifier != PROTOTYPE_INVALID_ID) {
		if (prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				classifier,
				result->projected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
		classifier = result->projected_classifier;
		result->solved_classifier = classifier;
	}
	uint32_t subjects[2] = { constraint->computation, constraint->continuation };
	uint32_t classifiers[2] = { input_classifier, continuation_classifier };
	uint32_t premise_occurrence_ids[2];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[2];
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		premise_occurrence_ids[0] = constraint->premise_occurrences[0];
		premise_occurrence_ids[1] = constraint->premise_occurrences[1];
		int premise_status = select_closed_constraint_premises(
				constraint,
				subjects,
				classifiers,
				2,
				selected_premises
			);
		if (premise_status != 0) {
			return premise_status < 0 ? -1 : 1;
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = input_evidence;
		selected_premises[1] = continuation_evidence;
		premise_occurrence_ids[0] = input_evidence.occurrence_id;
		premise_occurrence_ids[1] = continuation_evidence.occurrence_id;
		premise_evidence = selected_premises;
	}
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL, premise_occurrence_ids, subjects, classifiers, premise_evidence, 2
	);
}

static int solve_operation_request_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint,
	struct prototype_judgement_computation_constraint_result* result,
	uint32_t constraint_id
) {
	if (constraint->application == PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			if (terms->terms[i].tag == PROTOTYPE_TERM_APP &&
				terms->terms[i].as.app.function == constraint->computation &&
				terms->terms[i].as.app.argument == constraint->argument) {
				constraint->application = i;
				break;
			}
		}
		if (constraint->application == PROTOTYPE_INVALID_ID &&
			prototype_term_app(
				terms, constraint->computation, constraint->argument, &constraint->application
			) != 0) {
			return -1;
		}
	}
	uint32_t application_classifier;
	uint32_t argument_classifier;
	struct prototype_judgement_selected_evidence application_evidence;
	struct prototype_judgement_selected_evidence argument_evidence;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_occurrence_count < 3 ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID ||
			constraint->premise_classifiers[1] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		argument_classifier = constraint->premise_classifiers[1];
		if (prototype_judgement_delta_app_elim_classifier(
				delta,
				terms,
				type_declarations,
				constraint->premise_classifiers[0],
				constraint->argument,
				argument_classifier,
				&application_classifier
			) != 0) {
			return 1;
		}
	} else {
		if (lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->application,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
				&application_evidence
			) != 0) {
			int status = prototype_judgement_delta_expand_app(
				delta, terms, type_declarations, constraint->application, NULL
			);
			return status < 0 ? -1 : 1;
		}
		application_classifier = application_evidence.classifier;
		int argument_status = lookup_delta_app_elim_argument_evidence(
			delta,
			constraint->application,
			application_classifier,
			constraint->argument,
			&argument_evidence
		);
		if (argument_status != 0) {
			return argument_status < 0 ? -1 : 1;
		}
		argument_classifier = argument_evidence.classifier;
	}
	struct prototype_term_classifier_view operation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, application_classifier, &operation_view
		) != 0) {
		return -1;
	}
	/* A request is only meaningful after the operation application is Comp. */
	if (operation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (!term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_THUNK)) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[constraint->continuation].as.thunk.computation;
	if (!term_has_tag(terms, continuation_lambda, PROTOTYPE_TERM_LAMBDA)) {
		return -1;
	}
	delta->current_context_id = constraint->context_id;
	delta->current_occurrence_id = constraint->occurrence_id;
	uint32_t continuation_function_classifier;
	uint32_t continuation_classifier;
	struct prototype_judgement_selected_evidence continuation_evidence;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		continuation_classifier = constraint->premise_classifiers[2];
		if (continuation_classifier == PROTOTYPE_INVALID_ID ||
			!term_has_tag(terms, continuation_classifier, PROTOTYPE_TERM_THUNK_TYPE)) {
			return 1;
		}
		continuation_function_classifier =
			terms->terms[continuation_classifier].as.thunk_type.computation;
	} else {
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, continuation_lambda, operation_view.result
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence continuation_function_evidence;
		if (lookup_delta_selected_evidence(
				delta, continuation_lambda, 1, &continuation_function_evidence
			) != 0) {
			return 1;
		}
		continuation_function_classifier = continuation_function_evidence.classifier;
		if (prototype_term_thunk_type(
				terms, continuation_function_classifier, &continuation_classifier
			) != 0 || record_cbpv_boundary_with_evidence(
				delta,
				terms,
				type_declarations,
				constraint->continuation,
				continuation_function_evidence.occurrence_id,
				continuation_function_classifier,
				&continuation_function_evidence
			) != 0) {
			return -1;
		}
		if (lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				constraint->continuation,
				continuation_classifier,
				&continuation_evidence
			) != 0) {
			return 1;
		}
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &codomain_family) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, operation_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t continuation_binder = terms->terms[continuation_lambda].as.lambda.binding_id;
	uint32_t continuation_var;
	uint32_t continuation_result;
	if (prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			continuation_function_classifier, continuation_var, domain,
			PROTOTYPE_INVALID_ID,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, continuation_result, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (prototype_term_contains_free_binding(
			terms, continuation_view.result, continuation_binder
		)) {
		return 1;
	}
	uint32_t operation_effect_row = operation_view.effect_row;
	int operation_id;
	if (prototype_term_effect_operation_identity(
			terms, constraint->computation, &operation_id
		) != 0) {
		return -1;
	}
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!declaration) {
		return -1;
	}
	int has_suspended_argument;
	uint32_t operation_head = constraint->computation;
	while (operation_head < terms->term_count &&
		terms->terms[operation_head].tag == PROTOTYPE_TERM_APP) {
		operation_head = terms->terms[operation_head].as.app.function;
	}
	if (operation_head >= terms->term_count ||
		terms->terms[operation_head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	if (prototype_term_effect_operation_classifier_has_suspended_argument(
			terms,
			terms->terms[operation_head].as.effect_operation.classifier,
			&has_suspended_argument
		) != 0) {
		return -1;
	}
	if (has_suspended_argument) {
		if (!term_has_tag(terms, argument_classifier, PROTOTYPE_TERM_THUNK_TYPE)) {
			return -1;
		}
		struct prototype_term_classifier_view argument_view;
		if (prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				terms->terms[argument_classifier].as.thunk_type.computation,
				&argument_view
			) != 0 || argument_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			argument_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_operation(
				terms, operation_id, argument_view.effect_row, &operation_effect_row
			) != 0) {
			return -1;
		}
	}
	struct prototype_term_classifier_view request_operation_view = operation_view;
	request_operation_view.effect_row = operation_effect_row;
	uint32_t effect_row;
	uint32_t classifier;
	if (computation_effect_row_union(
			terms, &request_operation_view, &continuation_view, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, &classifier
		) != 0) {
		return -1;
	}
	uint32_t request_rows[2] = {
		operation_effect_row,
		continuation_view.effect_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN, constraint_id,
			constraint->subject, effect_row, request_rows, 2
		) != 0) {
		return -1;
	}
	/* Classifier synthesis is independent of proof closure. Publish the checked
	 * solution even when premise evidence is not available until a later pass. */
	result->solved_classifier = classifier;
	if (result->projected_classifier != PROTOTYPE_INVALID_ID) {
		if (prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				classifier,
				result->projected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
		classifier = result->projected_classifier;
		result->solved_classifier = classifier;
	}
	uint32_t subjects[3] = {
		constraint->application,
		constraint->argument,
		constraint->continuation
	};
	uint32_t classifiers[3] = {
		application_classifier,
		argument_classifier,
		continuation_classifier
	};
	delta->current_context_id = constraint->context_id;
	delta->current_occurrence_id = constraint->occurrence_id;
	uint32_t premise_occurrence_ids[3];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[3];
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		premise_occurrence_ids[0] = PROTOTYPE_INVALID_ID;
		premise_occurrence_ids[1] = constraint->premise_occurrences[1];
		premise_occurrence_ids[2] = constraint->premise_occurrences[2];
		memset(selected_premises, 0, sizeof(selected_premises));
		selected_premises[0] = (struct prototype_judgement_selected_evidence){
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			.authority_id = constraint->application,
			.context_id = constraint->context_id,
			.occurrence_id = PROTOTYPE_INVALID_ID,
			.subject = constraint->application,
			.classifier = application_classifier
		};
		for (uint32_t i = 1; i < 3; ++i) {
			if (constraint->premise_states[i] ==
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
				return 1;
			}
			if (constraint->premise_states[i] !=
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED ||
				constraint->premise_evidence[i].kind !=
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				constraint->premise_evidence[i].authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
				constraint->premise_evidence[i].subject != subjects[i] ||
				constraint->premise_evidence[i].classifier != classifiers[i]) {
				return -1;
			}
			selected_premises[i] = constraint->premise_evidence[i];
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = application_evidence;
		selected_premises[1] = argument_evidence;
		selected_premises[2] = continuation_evidence;
		for (uint32_t i = 0; i < 3; ++i) {
			premise_occurrence_ids[i] = selected_premises[i].occurrence_id;
		}
		premise_evidence = selected_premises;
	}
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
		NULL, premise_occurrence_ids, subjects, classifiers, premise_evidence, 3
	);
}

static int record_computation_lambda_body_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t body_occurrence_id,
	uint32_t lambda,
	uint32_t source_classifier,
	uint32_t target_effect_row,
	uint32_t* p_target_classifier
) {
	if (!delta || !delta->occurrences ||
		body_occurrence_id >= delta->occurrence_count || !terms ||
		!type_declarations || !p_target_classifier ||
		lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		source_classifier >= terms->term_count || target_effect_row >= terms->term_count ||
		!delta->contexts) {
		return -1;
	}
	const struct prototype_typed_occurrence* body_occurrence =
		&delta->occurrences[body_occurrence_id];
	uint32_t body_context_id = body_occurrence->context_id;
	uint32_t body_subject = body_occurrence->core_term;
	if (body_context_id >= delta->contexts->context_count ||
		body_subject >= terms->term_count) {
		return -1;
	}
	uint32_t source_pi;
	uint32_t domain;
	uint32_t source_family;
	uint32_t binder = terms->terms[lambda].as.lambda.binding_id;
	uint32_t binder_var;
	uint32_t source_body_classifier;
	uint32_t source_family_binder;
	uint32_t source_family_body;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, source_classifier,
			&source_pi, &domain, &source_family
		) != 0 || prototype_term_var(terms, binder, &binder_var) != 0 ||
		prototype_term_pure_family_parts(
			terms,
			source_family,
			&source_family_binder,
			&source_family_body
		) != 0) {
		return -1;
	}
	if (!prototype_term_contains_free_binding(
			terms, source_family_body, source_family_binder
		)) {
		source_body_classifier = source_family_body;
	} else if (pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			source_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&source_body_classifier
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view source_body;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_body_classifier, &source_body
		) != 0 || source_body.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source_body.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t target_body_classifier;
	if (prototype_term_computation_type(
			terms, target_effect_row, source_body.result, &target_body_classifier
		) != 0) {
		return -1;
	}
	uint32_t saved_context = delta->current_context_id;
	uint32_t saved_operation = delta->current_occurrence_id;
	prototype_judgement_delta_set_context(delta, body_context_id);
	delta->current_occurrence_id = body_occurrence_id;
	struct prototype_judgement_selected_evidence source_evidence;
	int source_status = prototype_judgement_delta_select_evidence(
		delta,
		body_occurrence_id,
		body_context_id,
		body_subject,
		source_body_classifier,
		&source_evidence
	);
	int weaken_status = source_status == 0 ?
		prototype_judgement_delta_record_effect_weaken(
			delta,
			terms,
			type_declarations,
			&source_evidence,
			target_body_classifier
		) : source_status;
	prototype_judgement_delta_set_context(delta, saved_context);
	delta->current_occurrence_id = saved_operation;
	if (weaken_status != 0) {
		return weaken_status < 0 ? -1 : 1;
	}
	(void)source_family;
	/* Weakening belongs to the computation returned by the lambda. The lambda's
	 * source Pi remains the return-clause derivation used by the fold rule. */
	*p_target_classifier = source_classifier;
	return 0;
}

static int fold_clause_closed_body_row(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t operation_domain,
	uint32_t carrier_result,
	uint32_t* p_effect_row
) {
	uint32_t outer_domain;
	uint32_t outer_family;
	uint32_t outer_binder;
	uint32_t inner_classifier;
	uint32_t inner_domain;
	uint32_t inner_family;
	uint32_t inner_binder;
	uint32_t body_classifier;
	uint32_t instantiated_domain;
	uint32_t instantiated_body;
	struct prototype_term_classifier_view body_view;
	if (!terms || !type_declarations || !p_effect_row ||
		pi_parts(terms, classifier, &outer_domain, &outer_family) != 0 ||
		prototype_term_pure_family_parts(
			terms, outer_family, &outer_binder, &inner_classifier
		) != 0 || pi_parts(
			terms, inner_classifier, &inner_domain, &inner_family
		) != 0 || prototype_term_pure_family_parts(
			terms, inner_family, &inner_binder, &body_classifier
		) != 0 || instantiate_fold_clause_classifier(
			terms,
			type_declarations,
			outer_domain,
			operation_domain,
			outer_domain,
			&instantiated_domain
		) != 0 || instantiate_fold_clause_classifier(
			terms,
			type_declarations,
			outer_domain,
			operation_domain,
			body_classifier,
			&instantiated_body
		) != 0) {
		return -1;
	}
	int domain_equal;
	int result_equal;
	if (classifier_conversion_decision(
			prototype_judgement_classifier_conversion(
				terms, type_declarations, instantiated_domain, operation_domain
			),
			&domain_equal
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, instantiated_body, &body_view
		) != 0 || body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		body_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		classifier_conversion_decision(
			prototype_judgement_classifier_conversion(
				terms, type_declarations, body_view.result, carrier_result
			),
			&result_equal
		) != 0) {
		return -1;
	}
	(void)outer_binder;
	(void)inner_domain;
	(void)inner_binder;
	if (!domain_equal || !result_equal ||
		(body_view.effect_row < terms->term_count &&
		 terms->terms[body_view.effect_row].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR)) {
		return 1;
	}
	*p_effect_row = body_view.effect_row;
	return 0;
}

static int solve_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint,
	struct prototype_judgement_computation_constraint_result* result,
	uint32_t constraint_id
) {
	if (constraint->subject >= terms->term_count ||
		terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[constraint->subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	if (clause_count == 0 ||
		clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	uint32_t input_classifier = PROTOTYPE_INVALID_ID;
	struct prototype_judgement_selected_evidence input_evidence;
	memset(&input_evidence, 0, sizeof(input_evidence));
	int input_status = -1;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_occurrence_count != 2 + 2 * clause_count ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		input_classifier = constraint->premise_classifiers[0];
		input_status = 0;
	} else if (term_has_tag(
			terms,
			fold->as.computation_fold.computation,
			PROTOTYPE_TERM_OPERATION_REQUEST
		)) {
		input_status = lookup_delta_selected_evidence_with_proof_kind(
			delta,
			fold->as.computation_fold.computation,
			PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
			&input_evidence
		);
	} else if (term_has_tag(
			terms,
			fold->as.computation_fold.computation,
			PROTOTYPE_TERM_COMPUTATION_FOLD
		)) {
		input_status = lookup_delta_selected_evidence_with_proof_kind(
			delta,
			fold->as.computation_fold.computation,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
			&input_evidence
		);
	} else {
		input_status = lookup_delta_selected_evidence(
			delta,
			fold->as.computation_fold.computation,
			1,
			&input_evidence
		);
	}
	if (input_status != 0) {
		return 1;
	}
	if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
		input_classifier = input_evidence.classifier;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}

	uint32_t operation_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t operation_declaration_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t operation_domains[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_term_classifier_view operation_views[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_judgement_selected_evidence operation_evidence[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t handled_operation_rows[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t handled_operations_row;
	if (prototype_term_effect_row_empty(terms, &handled_operations_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_pi;
		uint32_t operation_family;
		uint32_t operation_binder;
		uint32_t operation_var;
		uint32_t operation_result;
		uint32_t ignored_operation_body;
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			operation_classifiers[i] = constraint->premise_classifiers[2 + 2 * i];
			if (operation_classifiers[i] == PROTOTYPE_INVALID_ID) {
				return 1;
			}
		} else {
			if (lookup_delta_selected_evidence(
					delta, clause->operation, 1, &operation_evidence[i]
				) != 0) {
				return 1;
			}
			operation_classifiers[i] = operation_evidence[i].classifier;
		}
		operation_declaration_classifiers[i] = operation_classifiers[i];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				terms, clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		int specialization_status = prototype_judgement_specialize_fold_operation_classifier(
			terms,
			type_declarations,
			input_view.effect_row,
			operation_identity,
			operation_classifiers[i],
			&operation_classifiers[i]
		);
		if (specialization_status != 0) {
			return specialization_status < 0 ? -1 : 1;
		}
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, operation_classifiers[i],
				&operation_pi, &operation_domains[i], &operation_family
			) != 0 || prototype_term_pure_family_parts(
				terms, operation_family, &operation_binder, &ignored_operation_body
			) != 0 || prototype_term_var(terms, operation_binder, &operation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				operation_pi, operation_var, operation_domains[i],
				PROTOTYPE_INVALID_ID, &operation_result
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, operation_result, &operation_views[i]
			) != 0 || operation_views[i].category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_views[i].computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		uint32_t union_row;
		if (prototype_term_effect_row_union(
				terms, handled_operations_row, operation_views[i].effect_row, &union_row
			) != 0) {
			return -1;
		}
		handled_operation_rows[i] = operation_views[i].effect_row;
		handled_operations_row = union_row;
	}
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint_id,
			constraint->subject,
			handled_operations_row,
			handled_operation_rows,
			clause_count
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view handled_operations_view = {
		.category = PROTOTYPE_TERM_CATEGORY_COMPUTATION,
		.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING,
		.effect_row = handled_operations_row,
		.result = input_view.result
	};
	uint32_t residual_effect_row;
	int residual_status = computation_fold_residual_row(
		terms, &input_view, &handled_operations_view, &residual_effect_row
	);
	if (residual_status < 0) {
		return -1;
	}
	if (residual_status > 0) {
		result->effect_residual_pending = 1;
		result->effect_residual_row = PROTOTYPE_INVALID_ID;
		return 1;
	}
	result->effect_residual_pending = 0;
	result->effect_residual_row = residual_effect_row;
	uint32_t residual_rows[2] = {
		input_view.effect_row,
		handled_operations_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL, constraint_id,
			constraint->subject, residual_effect_row, residual_rows, 2
		) != 0) {
		return -1;
	}

	if (infer_lambda_classifier_for_app_argument(
			delta, terms, type_declarations, fold->as.computation_fold.return_clause, input_view.result
		) != 0) {
		return -1;
	}
	uint32_t return_classifier;
	struct prototype_judgement_selected_evidence return_evidence;
	int return_selection;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		return_classifier = constraint->premise_classifiers[1];
		return_selection = return_classifier == PROTOTYPE_INVALID_ID ? 1 : 0;
	} else {
		return_selection = select_delta_pi_classifier_for_domain(
			delta,
			terms,
			type_declarations,
			fold->as.computation_fold.return_clause,
			input_view.result,
			&return_evidence
		);
		if (return_selection == 0) {
			return_classifier = return_evidence.classifier;
		}
	}
	if (return_selection < 0) {
		return -1;
	}
	if (return_selection > 0) {
		return 1;
	}
	uint32_t return_pi;
	uint32_t return_domain;
	uint32_t return_family;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, return_classifier,
			&return_pi, &return_domain, &return_family
		) != 0) {
		return -1;
	}
	if (fold->as.computation_fold.return_clause >= terms->term_count ||
		terms->terms[fold->as.computation_fold.return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	uint32_t return_binder =
		terms->terms[fold->as.computation_fold.return_clause].as.lambda.binding_id;
	uint32_t return_var;
	uint32_t output_classifier;
	if (prototype_term_var(terms, return_binder, &return_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			return_pi, return_var, return_domain,
			PROTOTYPE_INVALID_ID, &output_classifier
		) != 0) {
		return -1;
	}
	int dependent_output = prototype_term_contains_free_binding(
		terms, output_classifier, return_binder
	);
	struct prototype_term_classifier_view output_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, output_classifier, &output_view
		) != 0 || output_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		output_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t carrier_rows[33];
	uint32_t carrier_row_count = 0;
	carrier_rows[carrier_row_count++] = output_view.effect_row;
	carrier_rows[carrier_row_count++] = residual_effect_row;
	uint32_t handled_effect_row;
	uint32_t handled_output_classifier;
	if (prototype_term_effect_row_union(
			terms, output_view.effect_row, residual_effect_row, &handled_effect_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			uint32_t clause_row;
			uint32_t premise = 3 + 2 * i;
			if (premise >= constraint->premise_occurrence_count ||
				constraint->premise_classifiers[premise] == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int row_status = fold_clause_closed_body_row(
				terms,
				type_declarations,
				constraint->premise_classifiers[premise],
				operation_domains[i],
				output_view.result,
				&clause_row
			);
			if (row_status < 0) {
				return -1;
			}
			if (row_status == 0) {
				uint32_t joined_row;
				if (prototype_term_effect_row_union(
						terms, handled_effect_row, clause_row, &joined_row
					) != 0) {
					return -1;
				}
				handled_effect_row = joined_row;
				carrier_rows[carrier_row_count++] = clause_row;
			}
			continue;
		}
		for (size_t relation_id = 0;
			relation_id < delta->proposition_count;
			++relation_id) {
			const struct prototype_judgement_proposition* relation =
				&delta->propositions[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != clause->body) {
				continue;
			}
			uint32_t clause_row;
			int row_status = fold_clause_closed_body_row(
				terms,
				type_declarations,
				relation->classifier,
				operation_domains[i],
				output_view.result,
				&clause_row
			);
			if (row_status != 0) {
				continue;
			}
			uint32_t joined_row;
			if (prototype_term_effect_row_union(
					terms,
					handled_effect_row,
					clause_row,
					&joined_row
				) != 0) {
				return -1;
			}
			handled_effect_row = joined_row;
			carrier_rows[carrier_row_count++] = clause_row;
			break;
		}
	}
	result->effect_output_row = handled_effect_row;
	if (prototype_term_computation_type(
			terms, handled_effect_row, output_view.result, &handled_output_classifier
		) != 0) {
		return -1;
	}
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN, constraint_id,
			constraint->subject,
			handled_effect_row,
			carrier_rows,
			carrier_row_count
		) != 0) {
		return -1;
	}
	uint32_t return_inclusion[2] = {
		output_view.effect_row,
		handled_effect_row
	};
	uint32_t residual_inclusion[2] = {
		residual_effect_row,
		handled_effect_row
	};
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint_id,
			constraint->subject,
			handled_effect_row,
			return_inclusion,
			2
		) != 0 || add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint_id,
			constraint->subject,
			handled_effect_row,
			residual_inclusion,
			2
		) != 0) {
		return -1;
	}
	if (!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			output_view.effect_row,
			handled_effect_row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		if (prototype_term_effect_row_is_closed(
				terms, output_view.effect_row
			) != 1 || prototype_term_effect_row_is_closed(
				terms, handled_effect_row
			) != 1) {
			/* The partial-policy solver retains this inclusion obligation. It must
			 * not fabricate a closed EFFECT_WEAKEN proof for a symbolic row. */
			return 1;
		}
		uint32_t widened_return_classifier;
		int weaken_status = record_computation_lambda_body_effect_weaken(
				delta,
				terms,
				type_declarations,
				constraint->return_body_occurrence_id,
				fold->as.computation_fold.return_clause,
				return_classifier,
				handled_effect_row,
				&widened_return_classifier
			);
		if (weaken_status != 0) {
			return weaken_status < 0 ? -1 : 1;
		}
		return_classifier = widened_return_classifier;
	}

	uint32_t outer_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_judgement_selected_evidence outer_evidence[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_domain = operation_domains[i];
		struct prototype_term_classifier_view operation_view = operation_views[i];
		if (clause->body >= terms->term_count ||
			terms->terms[clause->body].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t continuation_lambda =
			terms->terms[clause->body].as.lambda.body;
		if (continuation_lambda >= terms->term_count ||
			terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t continuation_function_classifier;
		uint32_t continuation_expected;
		if (prototype_term_pi(
				terms,
				operation_view.result,
				handled_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				terms, continuation_function_classifier, &continuation_expected
			) != 0) {
			return -1;
		}
		uint32_t saved_context = delta->current_context_id;
		uint32_t operation_argument_context;
		uint32_t continuation_context;
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				clause->body,
				operation_domain
			) != 0 || prototype_context_extend(
				delta->contexts,
				saved_context,
				terms->terms[clause->body].as.lambda.binding_id,
				operation_domain,
				PROTOTYPE_INVALID_ID,
				&operation_argument_context
			) != 0) {
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				continuation_lambda,
				continuation_expected
			) != 0 || prototype_context_extend(
				delta->contexts,
				operation_argument_context,
				terms->terms[continuation_lambda].as.lambda.binding_id,
				continuation_expected,
				PROTOTYPE_INVALID_ID,
				&continuation_context
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, continuation_context);
		uint32_t continuation_body = terms->terms[continuation_lambda].as.lambda.body;
		if (continuation_body < terms->term_count &&
			terms->terms[continuation_body].tag == PROTOTYPE_TERM_APP &&
			prototype_judgement_delta_expand_app(
				delta, terms, type_declarations, continuation_body, NULL
			) < 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, continuation_lambda, continuation_expected
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, saved_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t expected_continuation_classifier;
		uint32_t expected_outer_classifier;
		if (prototype_term_pi(
				terms,
				continuation_expected,
				handled_output_classifier,
				&expected_continuation_classifier
			) != 0 || prototype_term_pi(
				terms,
				operation_domain,
				expected_continuation_classifier,
				&expected_outer_classifier
			) != 0) {
			return -1;
		}
		uint32_t outer_classifier;
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			uint32_t outer_premise = 3 + 2 * i;
			outer_classifier = constraint->premise_classifiers[outer_premise];
			if (outer_classifier == PROTOTYPE_INVALID_ID ||
				constraint->premise_states[outer_premise] ==
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
				return 1;
			}
		} else {
			int outer_selection = lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				clause->body,
				expected_outer_classifier,
				&outer_evidence[i]
			);
			if (outer_selection < 0) {
				return -1;
			}
			if (outer_selection > 0) {
				return 1;
			}
			outer_classifier = outer_evidence[i].classifier;
		}
		uint32_t outer_pi;
		uint32_t outer_domain;
		uint32_t outer_family;
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, outer_classifier,
				&outer_pi, &outer_domain, &outer_family
			) != 0) {
			return -1;
		}
		uint32_t outer_binder;
		uint32_t outer_var;
		uint32_t continuation_classifier;
		uint32_t ignored_outer_body;
		if (prototype_term_pure_family_parts(
				terms, outer_family, &outer_binder, &ignored_outer_body
			) != 0 ||
			prototype_term_var(terms, outer_binder, &outer_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				outer_pi, outer_var, outer_domain,
				PROTOTYPE_INVALID_ID, &continuation_classifier
			) != 0 || continuation_classifier >= terms->term_count ||
			terms->terms[clause->body].as.lambda.body >= terms->term_count) {
			return -1;
		}
		uint32_t continuation_pi;
		uint32_t continuation_domain;
		uint32_t continuation_family;
		struct prototype_judgement_selected_evidence continuation_evidence;
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			continuation_classifier = expected_continuation_classifier;
		} else {
			int continuation_selection =
				lookup_delta_selected_evidence_normalization_equal(
					delta,
					terms,
					type_declarations,
					continuation_lambda,
					expected_continuation_classifier,
					&continuation_evidence
				);
			if (continuation_selection != 0) {
				return -1;
			}
			continuation_classifier = continuation_evidence.classifier;
		}
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, continuation_classifier,
				&continuation_pi, &continuation_domain, &continuation_family
			) != 0) {
			return -1;
		}
		uint32_t continuation_binder;
		uint32_t continuation_var;
		uint32_t continuation_output;
		uint32_t ignored_continuation_body;
		if (prototype_term_pure_family_parts(
				terms,
				continuation_family,
				&continuation_binder,
				&ignored_continuation_body
			) != 0 ||
			prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				continuation_pi, continuation_var, continuation_domain,
				PROTOTYPE_INVALID_ID, &continuation_output
			) != 0 || !(prototype_judgement_classifier_conversion(
				terms, type_declarations, continuation_output, handled_output_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		outer_classifiers[i] = outer_classifier;
	}

	if (dependent_output) {
		/* The return value needed to instantiate the computation-fold result family is
		 * produced by the handled computation.  The clause assumptions above
		 * are sufficient to classify the occurrence-local operation body, but
		 * no closed computation-fold classifier exists until runtime supplies that value.
		 * The source compiler records a computation-fold result obligation instead of
		 * publishing a closed fold-elimination proof. */
		return 1;
	}
	result->solved_classifier = handled_output_classifier;
	if (result->projected_classifier != PROTOTYPE_INVALID_ID) {
		if (prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				handled_output_classifier,
				result->projected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
		handled_output_classifier = result->projected_classifier;
		result->solved_classifier = handled_output_classifier;
	}
	uint32_t subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t classifiers[64];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	classifiers[0] = input_classifier;
	classifiers[1] = return_classifier;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
		classifiers[2 + 2 * i] = operation_declaration_classifiers[i];
		classifiers[3 + 2 * i] = outer_classifiers[i];
	}
	uint32_t premise_occurrence_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t premise_count = 2 + 2 * clause_count;
	if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < premise_count; ++i) {
			premise_occurrence_ids[i] = constraint->premise_occurrences[i];
		}
		int premise_status = select_closed_constraint_premises(
				constraint,
				subjects,
				classifiers,
				premise_count,
				selected_premises
			);
		if (premise_status != 0) {
			return premise_status < 0 ? -1 : 1;
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = input_evidence;
		selected_premises[1] = return_evidence;
		for (uint32_t i = 0; i < clause_count; ++i) {
			selected_premises[2 + 2 * i] = operation_evidence[i];
			selected_premises[3 + 2 * i] = outer_evidence[i];
		}
		for (uint32_t i = 0; i < premise_count; ++i) {
			premise_occurrence_ids[i] = selected_premises[i].occurrence_id;
		}
		premise_evidence = selected_premises;
	}
	/* Helper inference above is authority-neutral and may temporarily clear the
	 * active Operation. The solved constraint, not the last helper call, owns the
	 * fold conclusion. */
	delta->current_context_id = constraint->context_id;
	delta->current_occurrence_id = constraint->occurrence_id;
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject,
		handled_output_classifier, PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL, premise_occurrence_ids, subjects, classifiers, premise_evidence,
		premise_count
	);
}

static int solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity,
	int include_folds
) {
	if (!results || result_capacity < delta->computation_constraint_count) {
		return -1;
	}
	/* Request classifiers are premises of enclosing folds. Derive every request
	 * which is ready before allowing a fold to consume child classifiers. */
	size_t before_request_closure = delta->proposition_count;
	for (int pass = 0; pass < 2; ++pass) {
		if (pass == 1 && !include_folds) {
			break;
		}
		/* Operation-owned operand snapshots are refreshed by the frontend. If
		 * request closure changed their evidence, do not solve dependent folds
		 * against the old snapshot in this invocation. */
		if (pass == 1 && delta->proposition_count != before_request_closure) {
			break;
		}
		for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
			struct prototype_judgement_computation_constraint* constraint =
				&delta->computation_constraints[i];
			struct prototype_judgement_computation_constraint_result* result =
				&results[i];
			if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int is_request = constraint->kind ==
				PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
			if ((pass == 0) != is_request) {
				continue;
			}
			int budget_status = consume_solver_step(delta);
			if (budget_status != 0) {
				return budget_status;
			}
			delta->current_context_id = constraint->context_id;
			delta->current_occurrence_id = constraint->occurrence_id;
			if (!is_request) {
				if (constraint->kind != PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD) {
					return -1;
				}
			if (constraint->subject >= terms->term_count ||
				terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
				return -1;
			}
			const struct prototype_term* fold = &terms->terms[constraint->subject];
			int status = fold->as.computation_fold.clause_count == 0 ?
				solve_zero_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint, result
				) : solve_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint, result, (uint32_t)i
				);
			if (status < 0) {
				fprintf(
					stderr,
					"fold constraint failed operation=%u subject=%u\n",
					constraint->occurrence_id,
					constraint->subject
				);
				return -1;
			}
			} else {
			int status = solve_operation_request_constraint(
				delta, terms, type_declarations, constraint, result, (uint32_t)i
			);
			if (status < 0) {
				fprintf(
					stderr,
					"operation request constraint failed operation=%u subject=%u\n",
					constraint->occurrence_id,
					constraint->subject
				);
				return -1;
			}
			}
		}
	}
	return solve_effect_row_constraints(delta, terms);
}

static void reset_computation_constraint_solutions(
	struct prototype_judgement_delta* delta,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !results || result_capacity < delta->computation_constraint_count) {
		return;
	}
	/* Effect equations are outputs of this exact computation-constraint solve,
	 * not an append-only Judgement history. Retaining equations from an older
	 * operand snapshot makes final occurrence projection depend on solve order. */
	delta->effect_row_constraint_count = 0;
	for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
		results[i].solved_classifier = PROTOTYPE_INVALID_ID;
		results[i].effect_residual_pending = 0;
		results[i].effect_residual_row = PROTOTYPE_INVALID_ID;
		results[i].effect_output_row = PROTOTYPE_INVALID_ID;
	}
}

int prototype_judgement_delta_infer_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !terms || !type_declarations || !results ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}
	reset_computation_constraint_solutions(delta, results, result_capacity);

	for (;;) {
		size_t before = delta->proposition_count;
		if (prototype_judgement_delta_infer_cbpv_boundaries(
				delta, terms, type_declarations
			) != 0) {
			return -1;
		}
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations, results, result_capacity, 1
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count == before) {
			return 0;
		}
		reset_computation_constraint_solutions(delta, results, result_capacity);
	}
}

int prototype_judgement_delta_solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !terms || !type_declarations || !results ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}
	reset_computation_constraint_solutions(delta, results, result_capacity);
	for (;;) {
		size_t before = delta->proposition_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations, results, result_capacity, 1
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count == before) {
			return 0;
		}
		reset_computation_constraint_solutions(delta, results, result_capacity);
	}
}

int prototype_judgement_delta_solve_recorded_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !terms || !type_declarations || !results) {
		return -1;
	}
	/* Constraint solving may synthesize Core-level helper derivations. The
	 * owning request/fold Operation is reified separately from TypedOccurrenceGraph. */
	prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
	reset_computation_constraint_solutions(delta, results, result_capacity);
	return solve_computation_constraints(
		delta, terms, type_declarations, results, result_capacity, 1
	);
}

int prototype_judgement_delta_solve_recorded_computation_requests(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !terms || !type_declarations || !results) {
		return -1;
	}
	prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
	reset_computation_constraint_solutions(delta, results, result_capacity);
	return solve_computation_constraints(
		delta, terms, type_declarations, results, result_capacity, 0
	);
}

static int infer_core_helper_facts_with_results(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint_result* results,
	size_t result_capacity
) {
	if (!delta || !terms || !type_declarations || !results) {
		return -1;
	}
	/* This is deliberately an erased-Core closure pass. Its output may support
	 * motive/type synthesis, but cannot acquire the Operation authority that was
	 * active at the call site. Operation-owned evidence is rebuilt separately
	 * from exact TypedOccurrenceGraph edges. */
	prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
	size_t first_relation = delta->proposition_count;
	if (prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}
	reset_computation_constraint_solutions(delta, results, result_capacity);

	int changed = 1;
	while (changed) {
		changed = 0;
			uint32_t term_count = (uint32_t)terms->term_count;
			for (uint32_t i = 0; i < term_count; ++i) {
				uint32_t existing;
				if (terms->terms[i].tag != PROTOTYPE_TERM_LAMBDA &&
					terms->terms[i].tag != PROTOTYPE_TERM_APP &&
					lookup_delta_proven_classifier(delta, terms, i, &existing) == 0) {
					continue;
				}
				if (terms->terms[i].tag == PROTOTYPE_TERM_APP) {
					size_t before = delta->proposition_count;
					int status = prototype_judgement_delta_expand_app(
						delta,
						terms,
					type_declarations,
					i,
						NULL
					);
					if (status < 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
			} else if (terms->terms[i].tag == PROTOTYPE_TERM_LAMBDA) {
				if (infer_lambda_classifiers_from_body(
						delta,
						terms,
							type_declarations,
							i,
							&changed
						) != 0) {
						return -1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_RETURN ||
					terms->terms[i].tag == PROTOTYPE_TERM_THUNK ||
					terms->terms[i].tag == PROTOTYPE_TERM_FORCE) {
					size_t before = delta->proposition_count;
					int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
					if (status < 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_TEXT_LITERAL) {
					size_t before = delta->proposition_count;
					if (infer_text_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_INT_LITERAL) {
					size_t before = delta->proposition_count;
					if (infer_int_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
					size_t before = delta->proposition_count;
					if (infer_pure_primitive_classifier(
							delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
					size_t before = delta->proposition_count;
					if (infer_effect_operation_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR) {
					size_t before = delta->proposition_count;
				if (infer_constructor_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (type_instance_has_known_type(terms, type_declarations, i)) {
					size_t before = delta->proposition_count;
				if (infer_type_formation_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
				if (delta->proposition_count > before) {
					changed = 1;
				}
			}
		}
		size_t before = delta->proposition_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations, results, result_capacity, 1
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count > before) {
			changed = 1;
		}
	}
	for (size_t i = first_relation; i < delta->proposition_count; ++i) {
		if (delta->propositions[i].occurrence_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t cursor = 0;
		uint32_t derivation_id;
		int found = 0;
		while (prototype_judgement_candidate_derivation_next(
			delta->propositions,
			delta->proposition_count,
			delta->derivation_candidates,
			delta->derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			found = 1;
			if (!delta->derivation_candidates[derivation_id].conclusion ||
				delta->derivation_candidates[derivation_id].conclusion->occurrence_id !=
				PROTOTYPE_INVALID_ID) {
				return -1;
			}
		}
		if (!found) {
			return -1;
		}
	}
	return 0;
}

int prototype_judgement_delta_infer_core_helper_facts(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta) {
		return -1;
	}
	size_t result_capacity = delta->computation_constraint_capacity;
	if (result_capacity == 0) {
		result_capacity = 1;
	}
	struct prototype_judgement_computation_constraint_result* results = calloc(
		result_capacity,
		sizeof(struct prototype_judgement_computation_constraint_result)
	);
	if (!results) {
		return -1;
	}
	int status = infer_core_helper_facts_with_results(
		delta, terms, type_declarations, results, result_capacity
	);
	free(results);
	return status;
}
