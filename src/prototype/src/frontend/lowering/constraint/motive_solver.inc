/* Motive synthesis is typed compiler work above the context-free Core graph.
 * A branch index correspondence is derived from the indexed family telescope
 * and the solved branch substitution. Core shape is never used to discover a
 * replacement location. */

static const struct operation_constraint* operation_solver_motive_equation(
	const struct operation_constraint_db* constraints,
	uint32_t match_operation,
	uint32_t case_index
) {
	if (!constraints || match_operation >= 4096) {
		return NULL;
	}
	for (uint32_t i = constraints->first_motive_constraint[match_operation];
		i != PROTOTYPE_INVALID_ID;
		i = constraints->next_motive_constraint[i]) {
		if (i >= constraints->classifier_constraint_count) {
			return NULL;
		}
		const struct operation_constraint* goal =
			&constraints->constraints[i];
		if (goal->kind == OPERATION_CONSTRAINT_MOTIVE_EQUATION &&
			goal->payload.classifier.classifier_meta_id == match_operation &&
			goal->payload.classifier.goal.motive_case.case_index == case_index) {
			return goal;
		}
	}
	return NULL;
}

/* Recover a source-Context candidate from a term in a refined branch. A
 * branch substitution need not be invertible, so this is only candidate
 * generation: reindexing the result through the same substitution must recover
 * the original term before the caller may use it. */
static int operation_solver_match_case_constructor_classifier(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index,
	uint32_t* p_constructor,
	uint32_t* p_classifier
) {
	if (!ctx || !p_constructor || !p_classifier ||
		match_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		match_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		case_index >= match_operation->case_count) {
		return -1;
	}
	const struct prototype_term* match =
		&ctx->terms->terms[match_operation->core_term];
	uint32_t term_case_id = match->as.match.first_case + case_index;
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (term_case_id >= ctx->terms->case_count ||
		operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	const struct prototype_match_case* source_case =
		&ctx->terms->cases[term_case_id];
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (source_case->binder_count != operation_case->binder_count ||
		source_case->binder_count > 64 || prototype_term_constructor(
			ctx->terms, source_case->constructor_owner,
			source_case->constructor_id, p_constructor
		) != 0) {
		return -1;
	}
	uint32_t argument_classifiers[64];
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		uint32_t field;
		uint32_t binding_context;
		const struct prototype_context* binding_entry;
		if (prototype_term_var(
				ctx->terms, operation_case->binder_ids[i], &field
			) != 0 || prototype_term_app(
				ctx->terms, *p_constructor, field, p_constructor
			) != 0 || prototype_context_find_binding(
				&ctx->metadata->contexts, operation_case->context_id,
				operation_case->binder_ids[i], &binding_context
			) != 0 || !(binding_entry = prototype_context_get(
				&ctx->metadata->contexts, binding_context
			))) {
			return -1;
		}
		argument_classifiers[i] =
			prototype_context_classifier_term(binding_entry);
	}
	int saturated;
	int status = prototype_judgement_constructor_spine_classifier(
		ctx->terms, ctx->type_declarations,
		&ctx->metadata->contexts, &ctx->metadata->substitutions,
		operation_case->context_id, *p_constructor,
		source_case->constructor_owner, argument_classifiers,
		source_case->binder_count, p_classifier, &saturated
	);
	return status == PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID && saturated ?
		0 : status < 0 ? -1 : 1;
}

static int operation_solver_abstract_indexed_motive(
	struct compile_context* ctx,
	uint32_t value,
	uint32_t value_classifier,
	uint32_t result_classifier,
	uint32_t* p_motive
) {
	if (!ctx || !p_motive || value >= ctx->terms->term_count ||
		value_classifier >= ctx->terms->term_count ||
		result_classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[32];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			ctx->terms, value_classifier, &type_id, arguments, &argument_count
		) != 0 || type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return 1;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (declaration->index_count == 0 || declaration->index_count > 32 ||
		declaration->parameter_count + declaration->index_count != argument_count) {
		return 1;
	}
	uint32_t index_binders[32];
	uint32_t motive_body = result_classifier;
	int has_index_dependency = 0;
	for (uint32_t i = 0; i < declaration->index_count; ++i) {
		uint32_t index_variable;
		int dependency_found;
		index_binders[i] = prototype_term_new_binding(ctx->terms);
		if (index_binders[i] == PROTOTYPE_INVALID_ID || prototype_term_var(
				ctx->terms, index_binders[i], &index_variable
			) != 0) {
			return -1;
		}
		if (operation_motive_abstract_index(
				ctx, motive_body,
				arguments[declaration->parameter_count + i], index_variable,
				&motive_body, &dependency_found
			) != 0) {
			return -1;
		}
		if (dependency_found) {
			has_index_dependency = 1;
		}
	}
	uint32_t value_binder = prototype_term_new_binding(ctx->terms);
	uint32_t value_variable;
	if (value_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			ctx->terms, value_binder, &value_variable
		) != 0 || prototype_term_graph_replace_exact(
			ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), motive_body,
			value, value_variable, &motive_body
		) != 0 || prototype_term_lambda(
			ctx->terms, value_binder, motive_body, &motive_body
		) != 0) {
		return -1;
	}
	if (has_index_dependency) {
		for (uint32_t reverse = declaration->index_count; reverse > 0; --reverse) {
			if (prototype_term_lambda(
					ctx->terms, index_binders[reverse - 1], motive_body, &motive_body
				) != 0) {
				return -1;
			}
		}
	}
	uint32_t reinstantiated;
	int apply_status = prototype_judgement_apply_indexed_motive(
			ctx->terms, ctx->type_declarations, motive_body, value,
			value_classifier, 1, &reinstantiated
		);
	int reinstantiated_equal = apply_status == 0 &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, reinstantiated, result_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	if (apply_status != 0 || !reinstantiated_equal) {
		return 1;
	}
	*p_motive = motive_body;
	return 0;
}

/* Invert the same strictly-positive Pi lifting used by the kernel IH rule.
 * A direct recursive field has an indexed-family classifier. Acc-style fields
 * instead end in that family after one or more non-recursive Pi domains. Walk
 * the field and expected telescopes together, apply the field value to shared
 * fresh variables, then abstract the indexed motive at the recursive leaf. */
static int operation_solver_abstract_recursive_field_motive_inner(
	struct compile_context* ctx,
	uint32_t field_classifier,
	uint32_t field_value,
	uint32_t expected_classifier,
	uint32_t depth,
	uint32_t* p_motive
) {
	if (!ctx || !p_motive || field_classifier >= ctx->terms->term_count ||
		field_value >= ctx->terms->term_count ||
		expected_classifier >= ctx->terms->term_count || depth > 64) {
		return -1;
	}
	int direct_status = operation_solver_abstract_indexed_motive(
		ctx, field_value, field_classifier, expected_classifier, p_motive
	);
	if (direct_status <= 0) {
		return direct_status;
	}
	uint32_t field_whnf;
	uint32_t expected_whnf;
	if (prototype_judgement_classifier_value_whnf(
			ctx->terms, ctx->type_declarations, field_classifier, &field_whnf
		) != 0 || prototype_judgement_classifier_value_whnf(
			ctx->terms, ctx->type_declarations, expected_classifier, &expected_whnf
		) != 0) {
		return -1;
	}
	const struct prototype_term* field = &ctx->terms->terms[field_whnf];
	if (field->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		uint32_t expected_inner = expected_whnf < ctx->terms->term_count &&
			ctx->terms->terms[expected_whnf].tag == PROTOTYPE_TERM_THUNK_TYPE ?
			ctx->terms->terms[expected_whnf].as.thunk_type.computation :
			expected_classifier;
		return operation_solver_abstract_recursive_field_motive_inner(
			ctx, field->as.thunk_type.computation, field_value,
			expected_inner, depth + 1, p_motive
		);
	}
	if (field->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		struct prototype_term_classifier_view expected;
		int expected_status = prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL,
				expected_classifier, &expected
			);
		struct prototype_effect_row_normal_form field_row;
		if (prototype_term_effect_row_normal_form(
				ctx->terms, field->as.computation_type.label, &field_row
			) != 0) {
			return -1;
		}
		if (expected_status == 0 && expected.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			expected.computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			struct prototype_effect_row_normal_form expected_row;
			if (prototype_term_effect_row_normal_form(
					ctx->terms, expected.effect_row, &expected_row
				) != 0 || !prototype_term_effect_row_normal_form_includes(
					ctx->terms, &expected_row, &field_row
				)) {
				return 1;
			}
			return operation_solver_abstract_recursive_field_motive_inner(
				ctx, field->as.computation_type.result, field_value,
				expected.result, depth + 1, p_motive
			);
		}
		/* APP-side expectation opens a pure returning computation before
		 * applying the produced function. This is the inverse of the pure
		 * computation wrapper retained by recursive-field lifting. */
		if (field_row.atom_count != 0) {
			return 1;
		}
		return operation_solver_abstract_recursive_field_motive_inner(
			ctx, field->as.computation_type.result, field_value,
			expected_classifier, depth + 1, p_motive
		);
	}
	if (field->tag == PROTOTYPE_TERM_PI) {
		uint32_t field_domain;
		uint32_t field_family;
		uint32_t field_binder;
		uint32_t field_body;
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t expected_binder;
		uint32_t expected_body;
		if (prototype_judgement_pi_parts(
				ctx->terms, field_whnf, &field_domain, &field_family
			) != 0 || prototype_judgement_pi_parts(
				ctx->terms, expected_classifier, &expected_domain, &expected_family
			) != 0 || prototype_term_pure_family_parts(
				ctx->terms, field_family, &field_binder, &field_body
			) != 0 || prototype_term_pure_family_parts(
				ctx->terms, expected_family, &expected_binder, &expected_body
			) != 0 || !prototype_judgement_classifier_compatible(
				ctx->terms, ctx->type_declarations,
				field_domain, expected_domain
			)) {
			return 1;
		}
		uint32_t shared_binder = prototype_term_new_binding(ctx->terms);
		uint32_t shared_value;
		uint32_t next_field;
		uint32_t next_expected;
		uint32_t next_value;
		if (shared_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
				ctx->terms, shared_binder, &shared_value
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), field_body,
				field_binder, shared_value, &next_field
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), expected_body,
				expected_binder, shared_value, &next_expected
			) != 0 || prototype_term_app(
				ctx->terms, field_value, shared_value, &next_value
			) != 0) {
			return -1;
		}
		return operation_solver_abstract_recursive_field_motive_inner(
			ctx, next_field, next_value, next_expected, depth + 1, p_motive
		);
	}
	return 1;
}

static int operation_solver_abstract_recursive_field_motive(
	struct compile_context* ctx,
	uint32_t field_classifier,
	uint32_t field_value,
	uint32_t expected_classifier,
	uint32_t* p_motive
) {
	return operation_solver_abstract_recursive_field_motive_inner(
		ctx, field_classifier, field_value, expected_classifier, 0, p_motive
	);
}

static int operation_solver_induction_field_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		operation->ih_owner_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* owner =
		&ctx->metadata->typed_occurrences.occurrences[
			operation->ih_owner_occurrence
		];
	if (owner->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		owner->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[owner->core_term].tag != PROTOTYPE_TERM_MATCH ||
		operation->ih_case_index >=
			ctx->terms->terms[owner->core_term].as.match.case_count) {
		return -1;
	}
	uint32_t case_id = ctx->terms->terms[
		owner->core_term
	].as.match.first_case + operation->ih_case_index;
	if (case_id >= ctx->terms->case_count || operation->ih_field_index >=
		ctx->terms->cases[case_id].binder_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &ctx->terms->cases[case_id];
	uint32_t field_context = operation->context_id;
	uint32_t field_action = PROTOTYPE_INVALID_ID;
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			operation->context_action_substitution
		);
		if (!action || action->source_context != operation->context_id) {
			return -1;
		}
		field_context = action->target_context;
		field_action = operation->context_action_substitution;
	}
	uint32_t classifier;
	if (prototype_judgement_constructor_field_classifier(
			ctx->terms, ctx->type_declarations,
			&ctx->metadata->contexts, &ctx->metadata->substitutions,
			field_context, match_case->constructor_owner,
			match_case->constructor_id,
			&ctx->terms->case_binders[match_case->first_binder],
			operation->ih_field_index, operation->ih_field_index,
			&classifier
		) != 0 || (field_action != PROTOTYPE_INVALID_ID && prototype_term_reindex(
			ctx->terms, ctx->type_declarations,
			&ctx->metadata->contexts, &ctx->metadata->substitutions,
			classifier, field_action, &classifier
		) != 0)) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

/* A dependent sequence introduces a local binder for its runtime result.  A
 * Match motive cannot capture that branch-local binder: its equation must name
 * the prior computation instead.  For a proved-pure input, project the
 * computation to its symbolic value and substitute that value into the
 * classifier equation before abstracting the indexed motive.  Effectful or
 * unresolved inputs remain residual rather than being executed by typing. */
static int operation_solver_project_pure_sequence_results(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t* p_projected
) {
	if (!ctx || !p_projected || classifier >= ctx->terms->term_count ||
		operation_binder_owners_ensure(ctx) != 0) {
		return -1;
	}
	uint32_t projected = classifier;
	for (uint32_t i = 0; i < ctx->binder_owner_count; ++i) {
		struct operation_binder_owner_entry* owner =
			&ctx->binder_owners[i];
		if (owner->bound_computation_occurrence == PROTOTYPE_INVALID_ID ||
			!prototype_term_contains_free_binding(
				ctx->terms, projected, owner->binding_id
			)) {
			continue;
		}
		uint32_t computation_occurrence = owner->bound_computation_occurrence;
		if (computation_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* computation =
			&ctx->metadata->typed_occurrences.occurrences[
				computation_occurrence
			];
		uint32_t value = owner->projected_pure_value;
		if (value == PROTOTYPE_INVALID_ID) {
			uint64_t normalization_limit = compile_effort_remaining(ctx);
			int effort_status = compile_effort_charge(
				ctx, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
			);
			if (effort_status != 0) return effort_status;
			int projection_status = prototype_term_project_pure_computation_value(
				ctx->terms,
				ctx->type_declarations,
				computation->core_term,
				normalization_limit,
				&value
			);
			if (projection_status != 0) {
				return projection_status < 0 ? -1 : 1;
			}
			owner->projected_pure_value = value;
		}
		uint32_t computation_classifier = operation_solver_classifier(
			ctx, computation_occurrence
		);
		if (computation_classifier != PROTOTYPE_INVALID_ID) {
			struct prototype_term_classifier_view computation_view;
			if (prototype_judgement_classifier_view(
					ctx->terms, ctx->type_declarations, NULL,
					computation_classifier, &computation_view
				) != 0 || computation_view.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				computation_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			int purity = prototype_term_effect_row_purity(
				ctx->terms, computation_view.effect_row
			);
			if (purity == PROTOTYPE_EFFECT_ROW_PURITY_INVALID) {
				return -1;
			}
			if (purity == PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL) {
				return 1;
			}
		}
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				projected,
				owner->binding_id,
				value,
				&projected
			) != 0) {
			return -1;
		}
	}
	*p_projected = projected;
	return 0;
}

static int operation_solver_seed_indexed_motive_from_ih_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	int* p_changed
) {
	if (!ctx || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		expected_classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	uint32_t argument_occurrence;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		operation->ih_owner_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
			&argument_occurrence
		) != 0) {
		return -1;
	}
	uint32_t owner_match = operation->ih_owner_occurrence;
	uint32_t argument_classifier = operation_solver_classifier(
		ctx, argument_occurrence
	);
	if (operation_solver_classifier_in_occurrence_context(
			ctx, argument_occurrence, argument_classifier, &argument_classifier
		) != 0) {
		return -1;
	}
	uint32_t result_classifier = expected_classifier;
	uint32_t owner_branch_occurrence;
	struct prototype_term_classifier_view expected_view;
	if (compile_occurrence_child(
			ctx, owner_match, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
			operation->ih_case_index, &owner_branch_occurrence
		) != 0) {
		return -1;
	}
	/* The equation classifies the owner branch, not the local IH use. A value
	 * branch (for example append's nested Lambda) consumes the result of a pure
	 * local computation, while a computation branch (for example Acc's fold)
	 * retains Comp(E, A) as part of its motive. */
	if (ctx->metadata->typed_occurrences.occurrences[
			owner_branch_occurrence
		].tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL,
			expected_classifier, &expected_view
		) == 0 && expected_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		expected_view.computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		result_classifier = expected_view.result;
	}
	int projection_status = operation_solver_project_pure_sequence_results(
		ctx, result_classifier, &result_classifier
	);
	if (projection_status != 0) {
		return projection_status;
	}
	uint32_t motive;
	int status = operation_solver_abstract_indexed_motive(
		ctx,
		ctx->metadata->typed_occurrences.occurrences[argument_occurrence].core_term,
		argument_classifier,
		result_classifier,
		&motive
	);
	if (status > 0) {
		uint32_t field_classifier;
		if (operation_solver_induction_field_classifier(
				ctx, operation_id, &field_classifier
			) != 0) {
			return -1;
		}
		status = operation_solver_abstract_recursive_field_motive(
			ctx,
			field_classifier,
			ctx->metadata->typed_occurrences.occurrences[
				argument_occurrence
			].core_term,
			result_classifier,
			&motive
		);
	}
	if (status != 0) {
		return status;
	}
	const struct prototype_typed_occurrence* owner =
		&ctx->metadata->typed_occurrences.occurrences[owner_match];
	for (uint32_t binding_id = 0;
		binding_id < ctx->terms->next_binding_id;
		++binding_id) {
		if (!prototype_term_contains_free_binding(
				ctx->terms, motive, binding_id
			) || prototype_context_contains_binding(
				&ctx->metadata->contexts, owner->context_id, binding_id
			)) {
			continue;
		}
		int effect_owned = 0;
		for (uint32_t row = 0; row < owner->implicit_effect_row_count; ++row) {
			if (owner->implicit_effect_row_binders[row] == binding_id) {
				effect_owned = 1;
				break;
			}
		}
		if (!effect_owned) {
			/* A motive is rooted outside every branch continuation. Reject an
			 * equation that accidentally captures a branch-local APP or sequence
			 * binder and wait for the enclosing constructor equation to abstract it. */
			return 1;
		}
	}
	uint32_t previous_motive =
		ctx->classifier_solver.motive_solutions[owner_match].motive;
	int same_motive = 0;
	if (previous_motive < ctx->terms->term_count &&
		prototype_term_core_shape_equal_for_link(
			ctx->terms,
			ctx->type_declarations,
			previous_motive,
			ctx->terms,
			ctx->type_declarations,
			motive,
			&same_motive
		) != 0) {
		return -1;
	}
	uint8_t motive_state =
		ctx->classifier_solver.motive_solutions[owner_match].state;
	int motive_has_joint_equations =
		motive_state == OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY ||
		motive_state == OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
	if (motive_has_joint_equations && !same_motive) {
		/* A second recursive consumer contributes another equation for the selected
		 * motive. It cannot replace that family with a branch-local expectation. */
		return 1;
	}
	if (!same_motive || !motive_has_joint_equations) {
		/* IH consumer classifiers are provisional equations until the owner motive
		 * validates every branch. Keep all of them available so multiple recursive
		 * calls can join in one fixed-point round; materialization later replaces
		 * each projection from the selected motive and rejects any mismatch. */
		operation_motive_clear_recursive_equations(ctx, owner_match);
		ctx->classifier_solver.motive_solutions[owner_match].motive = motive;
		ctx->classifier_solver.motive_solutions[owner_match].state =
			OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, owner_match) != 0) {
			return -1;
		}
	}
	return operation_motive_register_recursive_equation(
		ctx, owner_match, operation_id
	);
}

/* A saturated constructor branch can determine an indexed motive before its
 * recursive fields have classifiers.  Abstract the constructor's indices from
 * the provisional branch result, then validate the abstraction by applying it
 * back to the same constructor occurrence.  This is solver-side synthesis;
 * no surface ascription participates. */
static int operation_solver_seed_indexed_motive_from_branch(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t source_case_index,
	uint32_t classifier,
	struct operation_motive_candidate* p_candidate
) {
	if (!ctx || !p_candidate || operation >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	*p_candidate = (struct operation_motive_candidate) {
		.motive = PROTOTYPE_INVALID_ID,
		.source_case_index = source_case_index
	};
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[operation];
	uint32_t scrutinee_operation;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		compile_occurrence_child(
			ctx, operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0 ||
		match_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_operation->core_term].tag !=
			PROTOTYPE_TERM_MATCH ||
		source_case_index >= match_operation->case_count) {
		return -1;
	}
	const struct prototype_term* match =
		&ctx->terms->terms[match_operation->core_term];
	uint32_t term_case_id = match->as.match.first_case + source_case_index;
	uint32_t operation_case_id =
		match_operation->first_case + source_case_index;
	if (term_case_id >= ctx->terms->case_count ||
		operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	const struct prototype_match_case* source_case =
		&ctx->terms->cases[term_case_id];
	const struct prototype_typed_occurrence_match_case* source_operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (source_case->binder_count != source_operation_case->binder_count ||
		source_case->binder_count > 64) {
		return -1;
	}

	uint32_t constructor_term;
	uint32_t argument_classifiers[64];
	if (prototype_term_constructor(
			ctx->terms, source_case->constructor_owner,
			source_case->constructor_id, &constructor_term
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		uint32_t field;
		uint32_t binding_context;
		const struct prototype_context* binding_entry;
		if (prototype_term_var(
				ctx->terms, source_operation_case->binder_ids[i], &field
			) != 0 || prototype_term_app(
				ctx->terms, constructor_term, field, &constructor_term
			) != 0 || prototype_context_find_binding(
				&ctx->metadata->contexts, source_operation_case->context_id,
				source_operation_case->binder_ids[i], &binding_context
			) != 0 || !(binding_entry = prototype_context_get(
				&ctx->metadata->contexts, binding_context
			))) {
			return -1;
		}
		argument_classifiers[i] =
			prototype_context_classifier_term(binding_entry);
	}
	uint32_t constructor_classifier;
	int constructor_saturated;
	if (prototype_judgement_constructor_spine_classifier(
			ctx->terms, ctx->type_declarations,
			&ctx->metadata->contexts, &ctx->metadata->substitutions,
			source_operation_case->context_id, constructor_term,
			source_case->constructor_owner, argument_classifiers,
			source_case->binder_count, &constructor_classifier,
			&constructor_saturated
		) != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
		!constructor_saturated) {
		return 1;
	}

	uint32_t scrutinee_classifier = operation_solver_classifier(
		ctx, scrutinee_operation
	);
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	uint32_t constructor_type;
	uint32_t constructor_arguments[32];
	uint32_t constructor_argument_count;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) != 0 || prototype_term_type_instance_info(
			ctx->terms, constructor_classifier, &constructor_type,
			constructor_arguments, &constructor_argument_count
		) != 0 || scrutinee_type != constructor_type ||
		scrutinee_argument_count != constructor_argument_count ||
		scrutinee_type >= ctx->type_declarations->semantic_schema.type_count) {
		return 1;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type];
	if (declaration->index_count == 0 ||
		declaration->parameter_count + declaration->index_count !=
			constructor_argument_count || declaration->index_count > 32) {
		return 1;
	}

	struct operation_motive_index_correspondence correspondences[32];
	for (uint32_t i = 0; i < declaration->index_count; ++i) {
		int correspondence_status = operation_motive_branch_index_correspondence(
			ctx,
			operation,
			source_case_index,
			constructor_classifier,
			i,
			&correspondences[i]
		);
		if (correspondence_status != OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE) {
			if (correspondence_status == OPERATION_MOTIVE_CORRESPONDENCE_INVALID) {
				return -1;
			}
			if (correspondence_status ==
				OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE) {
				p_candidate->unavailable_mask |= UINT64_C(1) << i;
			} else {
				p_candidate->conflict_mask |= UINT64_C(1) << i;
			}
			return correspondence_status;
		}
		if (i > 0 && correspondences[i].refinement_substitution !=
				correspondences[0].refinement_substitution) {
			return -1;
		}
	}
	uint32_t quoted_classifier;
	int quote_status = operation_motive_quote_refined_term(
		ctx,
		classifier,
		correspondences[0].refinement_substitution,
		&quoted_classifier
	);
	if (quote_status != 0) {
		return quote_status < 0 ? -1 : 1;
	}

	uint32_t index_binders[32];
	uint32_t motive_body = quoted_classifier;
	uint64_t dependency_mask = 0;
	for (uint32_t i = 0; i < declaration->index_count; ++i) {
		uint32_t index_variable;
		int dependency_found;
		index_binders[i] = prototype_term_new_binding(ctx->terms);
		if (index_binders[i] == PROTOTYPE_INVALID_ID || prototype_term_var(
				ctx->terms, index_binders[i], &index_variable
			) != 0 || operation_motive_abstract_index(
				ctx, motive_body,
				correspondences[i].source_index,
				index_variable, &motive_body, &dependency_found
			) != 0) {
			return -1;
		}
		if (dependency_found) {
			dependency_mask |= UINT64_C(1) << i;
		}
	}
	uint32_t value_binder = prototype_term_new_binding(ctx->terms);
	uint32_t value_variable;
	uint32_t body_before_value = motive_body;
	uint32_t value_abstracted_body;
	if (value_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
			ctx->terms, value_binder, &value_variable
		) != 0 || prototype_term_graph_replace_exact(
			ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), motive_body,
			match->as.match.scrutinee, value_variable, &value_abstracted_body
		) != 0 || prototype_term_lambda(
			ctx->terms, value_binder, value_abstracted_body, &motive_body
		) != 0) {
		return -1;
	}
	if (value_abstracted_body != body_before_value) {
		dependency_mask |= UINT64_C(1) << declaration->index_count;
	}
	for (uint32_t reverse = declaration->index_count; reverse > 0; --reverse) {
		if (prototype_term_lambda(
				ctx->terms, index_binders[reverse - 1], motive_body,
				&motive_body
			) != 0) {
			return -1;
		}
	}
	uint32_t branch_motive;
	uint32_t reinstantiated;
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			motive_body,
			correspondences[0].refinement_substitution,
			&branch_motive
		) != 0 || prototype_judgement_apply_indexed_motive(
			ctx->terms, ctx->type_declarations, branch_motive,
			constructor_term, constructor_classifier, 1, &reinstantiated
		) != 0 || prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, reinstantiated, classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	p_candidate->motive = motive_body;
	p_candidate->dependency_mask = dependency_mask;
	return 0;
}

static int operation_solver_seed_motive(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	uint32_t source_body_occurrence,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->classifier_solver.motive_solutions[operation].state ==
			OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY &&
		ctx->classifier_solver.motive_solutions[operation].motive <
			ctx->terms->term_count) {
		/* A guarded recursive equation determines the indexed family. A single
		 * constructor branch is only a provisional seed and cannot replace that
		 * jointly validated equation. */
		return 0;
	}
	const struct operation_constraint* source_equation = NULL;
	uint32_t source_case_index = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0;
		case_index < ctx->metadata->typed_occurrences.occurrences[operation].case_count;
		++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation, case_index
			);
		if (!equation) {
			return -1;
		}
		if (equation->payload.classifier.goal.motive_case.branch_operation == source_body_occurrence) {
			source_equation = equation;
			source_case_index = case_index;
			break;
		}
	}
	if (!source_equation) {
		return -1;
	}
	struct operation_motive_solution* solution =
		&ctx->classifier_solver.motive_solutions[operation];
	if (solution->state == OPERATION_MOTIVE_SOLUTION_PROVISIONAL &&
		solution->source_case_index == source_case_index &&
		solution->source_classifier == classifier) {
		return 0;
	}
	uint32_t classifier_whnf;
	int capture_status = operation_classifier_captures_case_binder(
		ctx, source_equation, classifier, &classifier_whnf
	);
	if (capture_status < 0) {
		return -1;
	}
	/* An indexed constructor result is itself an equation for the motive, even
	 * when the selected branch has no field binders (for example Vec.nil). Try
	 * the index abstraction first and validate it by reapplying it to the same
	 * constructor occurrence. Binder capture only determines whether failure to
	 * build that abstraction is a contradiction or permits the constant-motive
	 * fallback below. */
	struct operation_motive_candidate indexed_candidate;
	int indexed_status = operation_solver_seed_indexed_motive_from_branch(
		ctx, operation, source_case_index, classifier, &indexed_candidate
	);
	if (indexed_status == OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE ||
		indexed_status == OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT) {
		ctx->classifier_solver.motive_solutions[operation].unavailable_mask |=
			indexed_candidate.unavailable_mask;
		ctx->classifier_solver.motive_solutions[operation].conflict_mask |=
			indexed_candidate.conflict_mask;
	}
	if (indexed_status < 0 || (indexed_status > 0 && capture_status > 0)) {
		return indexed_status < 0 ? -1 : 1;
	}
	if (indexed_status == 0) {
		uint32_t indexed_motive = indexed_candidate.motive;
		uint32_t previous_motive =
			ctx->classifier_solver.motive_solutions[operation].motive;
		int same_motive = 0;
		if (previous_motive < ctx->terms->term_count &&
			prototype_term_core_shape_equal_for_link(
				ctx->terms,
				ctx->type_declarations,
				previous_motive,
				ctx->terms,
				ctx->type_declarations,
				indexed_motive,
				&same_motive
			) != 0) {
			return -1;
		}
		if (ctx->classifier_solver.motive_solutions[operation].state ==
				OPERATION_MOTIVE_SOLUTION_PROVISIONAL && !same_motive) {
			ctx->classifier_solver.motive_solutions[operation].ambiguity = 1;
			return 1;
		}
		if (ctx->classifier_solver.motive_solutions[operation].state !=
				OPERATION_MOTIVE_SOLUTION_PROVISIONAL) {
			ctx->classifier_solver.motive_solutions[operation].motive = indexed_motive;
			ctx->classifier_solver.motive_solutions[operation].dependency_mask =
				indexed_candidate.dependency_mask;
			ctx->classifier_solver.motive_solutions[operation].unavailable_mask =
				indexed_candidate.unavailable_mask;
			ctx->classifier_solver.motive_solutions[operation].conflict_mask =
				indexed_candidate.conflict_mask;
			ctx->classifier_solver.motive_solutions[operation].source_case_index =
				indexed_candidate.source_case_index;
			ctx->classifier_solver.motive_solutions[operation].source_classifier =
				classifier;
			ctx->classifier_solver.motive_solutions[operation].ambiguity = 0;
			ctx->classifier_solver.motive_solutions[operation].state =
				OPERATION_MOTIVE_SOLUTION_PROVISIONAL;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
				return -1;
			}
		} else {
			ctx->classifier_solver.motive_solutions[operation].dependency_mask |=
				indexed_candidate.dependency_mask;
		}
		return 0;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[operation];
	const struct prototype_term* match =
		&ctx->terms->terms[match_operation->core_term];
	if (source_case_index >= match->as.match.case_count) {
		return -1;
	}
	const struct prototype_match_case* source_case = &ctx->terms->cases[
		match->as.match.first_case + source_case_index
	];
	const struct prototype_typed_occurrence_match_case* source_operation_case =
		&ctx->metadata->typed_occurrences.cases[
			match_operation->first_case + source_case_index
		];
	if (source_operation_case->binder_count != source_case->binder_count) {
		return -1;
	}
	uint32_t constructor_term;
	if (prototype_term_constructor(
			ctx->terms, source_case->constructor_owner,
			source_case->constructor_id, &constructor_term
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(
				ctx->terms,
				source_operation_case->binder_ids[i],
				&field
			) != 0 || prototype_term_app(
				ctx->terms, constructor_term, field, &constructor_term
			) != 0) {
			return -1;
		}
	}
	uint32_t motive_binding = prototype_term_new_binding(ctx->terms);
	uint32_t motive_variable;
	uint32_t motive_body;
	uint32_t reinstantiated;
	if (motive_binding != PROTOTYPE_INVALID_ID &&
		prototype_term_var(ctx->terms, motive_binding, &motive_variable) == 0 &&
		prototype_term_graph_replace_exact(
			ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), classifier,
			constructor_term, motive_variable, &motive_body
		) == 0 && motive_body != classifier &&
		prototype_term_graph_substitute_bound_var(
			ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), motive_body, motive_binding,
			constructor_term, &reinstantiated
		) == 0 && prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, reinstantiated, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		uint32_t motive;
		if (prototype_term_lambda(
				ctx->terms, motive_binding, motive_body, &motive
			) != 0) {
			return -1;
		}
		if (ctx->classifier_solver.motive_solutions[operation].motive != motive ||
			ctx->classifier_solver.motive_solutions[operation].state !=
				OPERATION_MOTIVE_SOLUTION_PROVISIONAL) {
			ctx->classifier_solver.motive_solutions[operation].motive = motive;
			ctx->classifier_solver.motive_solutions[operation].dependency_mask =
				UINT64_C(1);
			ctx->classifier_solver.motive_solutions[operation].source_case_index =
				source_case_index;
			ctx->classifier_solver.motive_solutions[operation].source_classifier =
				classifier;
			ctx->classifier_solver.motive_solutions[operation].ambiguity = 0;
			ctx->classifier_solver.motive_solutions[operation].state =
				OPERATION_MOTIVE_SOLUTION_PROVISIONAL;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
				return -1;
			}
		}
		return 0;
	}
	uint32_t previous =
		ctx->classifier_solver.motive_solutions[operation].constant_candidate;
	if (previous == PROTOTYPE_INVALID_ID) {
		ctx->classifier_solver.motive_solutions[operation].constant_candidate =
			classifier_whnf;
		ctx->classifier_solver.motive_solutions[operation].dependency_mask = 0;
		ctx->classifier_solver.motive_solutions[operation].ambiguity = 0;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
			return -1;
		}
		/* A non-recursive equation establishes a constant motive independently of
		 * the still-open branches. Propagate that solved equation through those
		 * operation trees so guarded IH results can be constrained by their known
		 * consumers. This remains solver-side synthesis; a surface ascription is
		 * never used as an input. */
		for (uint32_t case_index = 0;
			case_index < match_operation->case_count; ++case_index) {
			const struct operation_constraint* equation =
				operation_solver_motive_equation(
					&ctx->constraints, operation, case_index
				);
			if (!equation) {
				return -1;
			}
			uint32_t branch_operation =
				equation->payload.classifier.goal.motive_case.branch_operation;
			if (branch_operation == source_body_occurrence ||
				operation_solver_classifier(ctx, branch_operation) !=
					PROTOTYPE_INVALID_ID) {
				continue;
			}
			int refine_status = operation_refine_classifier_from_expected(
				ctx, branch_operation, classifier_whnf, 0, p_changed
			);
			if (refine_status < 0) {
				/* A one-branch constant seed is provisional. A recursive branch
				 * that cannot consume it invalidates this candidate and lets the
				 * owner construct its guarded motive from all equations. */
				ctx->classifier_solver.motive_solutions[operation].constant_candidate =
					PROTOTYPE_INVALID_ID;
				ctx->classifier_solver.motive_solutions[operation].ambiguity = 1;
				return 1;
			}
		}
		uint32_t ih_scope = PROTOTYPE_INVALID_ID;
		if (match_operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[match_operation->core_term].tag ==
				PROTOTYPE_TERM_MATCH) {
			ih_scope =
				ctx->terms->terms[match_operation->core_term].as.match.ih_scope_id;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->constraints.classifier_constraint_count;
			++constraint_id) {
			const struct operation_constraint* constraint =
				&ctx->constraints.constraints[constraint_id];
			if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED ||
				constraint->payload.classifier.classifier_meta_id >= ctx->metadata->typed_occurrences.occurrence_count ||
				ctx->metadata->typed_occurrences.occurrences[constraint->payload.classifier.classifier_meta_id].ih_scope_id !=
					ih_scope) {
				continue;
			}
			if (operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
		return 0;
	}
	uint32_t joined;
	int join_status = operation_solver_join_match_classifiers(
		ctx, previous, classifier_whnf, &joined
	);
	if (join_status != 0) {
		if (join_status > 0) {
			ctx->classifier_solver.motive_solutions[operation].ambiguity = 1;
		}
		return join_status < 0 ? -1 : 1;
	}
	ctx->classifier_solver.motive_solutions[operation].ambiguity = 0;
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, joined
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		ctx->classifier_solver.motive_solutions[operation].constant_candidate = joined;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_set_ih_motive_application(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		motive_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		argument_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph;
	struct prototype_typed_occurrence_induction_edge edge;
	graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	if (prototype_typed_occurrence_graph_induction_edge(
			graph, ctx->terms, operation, &edge
		) != 0 || edge.owner_match_occurrence != motive_operation ||
		edge.argument_occurrence != argument_occurrence) {
		return -1;
	}
	uint32_t previous =
		ctx->classifier_solver.ih_motive_application_ids[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		if (ctx->classifier_solver.motive_application_count >= 4096) {
			return -1;
		}
		ctx->classifier_solver.motive_applications[
			ctx->classifier_solver.motive_application_count
		] = (struct operation_solver_motive_application){
			motive_operation,
			argument_occurrence,
			edge.scope_id,
			edge.case_index,
			edge.field_index
		};
		ctx->classifier_solver.ih_motive_application_ids[operation] =
			ctx->classifier_solver.motive_application_count++;
		*p_changed = 1;
		return 0;
	}
	if (previous >= ctx->classifier_solver.motive_application_count) {
		return -1;
	}
	const struct operation_solver_motive_application* application =
		&ctx->classifier_solver.motive_applications[previous];
	return application->motive_operation == motive_operation &&
		application->argument_occurrence == argument_occurrence &&
		application->scope_frame == edge.scope_id &&
		application->case_index == edge.case_index &&
		application->field_index == edge.field_index ? 0 : -1;
}


static int operation_motive_quote_refined_term(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t substitution_id,
	uint32_t* p_quoted
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			ctx ? &ctx->metadata->substitutions : NULL, substitution_id
		);
	if (!ctx || !p_quoted || term >= ctx->terms->term_count || !substitution) {
		return -1;
	}
	uint32_t path[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			prototype_context_empty(&ctx->metadata->contexts),
			substitution->target_context,
			path,
			512,
			&path_count
		) != 0) {
		return -1;
	}
	uint32_t quoted = term;
	/* Replace deeper, more dependent images first. This avoids rewriting a
	 * subterm of a later substitution image before that image is recognized. */
	for (uint32_t reverse = path_count; reverse > 0; --reverse) {
		const struct prototype_context* entry = prototype_context_get(
			&ctx->metadata->contexts, path[reverse - 1]
		);
		uint32_t source_variable;
		uint32_t image;
		uint32_t next;
		if (!entry || entry->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, entry->binding_id, &source_variable
			) != 0 || prototype_substitution_binding_term(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				substitution_id,
				entry->binding_id,
				&image
			) != 0) {
			return -1;
		}
		if (image == source_variable) {
			continue;
		}
		if (prototype_term_graph_replace_exact(
				ctx->terms,
				prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				quoted,
				image,
				source_variable,
				&next
			) != 0) {
			return -1;
		}
		quoted = next;
	}
	uint32_t reinstantiated;
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			quoted,
			substitution_id,
			&reinstantiated
		) != 0) {
		return -1;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, reinstantiated, term
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	*p_quoted = quoted;
	return 0;
}

static int operation_motive_branch_index_correspondence(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index,
	uint32_t constructor_classifier,
	uint32_t index_ordinal,
	struct operation_motive_index_correspondence* p_correspondence
) {
	if (!ctx || !p_correspondence || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	uint32_t scrutinee_operation;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		case_index >= match_operation->case_count || compile_occurrence_child(
			ctx, match_operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (!prototype_typed_occurrence_match_case_is_solved(operation_case)) {
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}
	const struct prototype_substitution* refinement = prototype_substitution_get(
		&ctx->metadata->substitutions,
		operation_case->refinement_substitution
	);
	if (!refinement || refinement->source_context != operation_case->context_id ||
		!prototype_context_is_ancestor(
			&ctx->metadata->contexts,
			match_operation->context_id,
			refinement->target_context
		)) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}

	uint32_t scrutinee_classifier = operation_solver_classifier(
		ctx, scrutinee_operation
	);
	uint32_t family_type_id;
	uint32_t source_arguments[32];
	uint32_t source_argument_count;
	uint32_t constructor_type_id;
	uint32_t branch_arguments[32];
	uint32_t branch_argument_count;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &family_type_id,
			source_arguments, &source_argument_count
		) != 0 || prototype_term_type_instance_info(
			ctx->terms, constructor_classifier, &constructor_type_id,
			branch_arguments, &branch_argument_count
		) != 0 || family_type_id != constructor_type_id || family_type_id >=
			ctx->type_declarations->semantic_schema.type_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[family_type_id];
	if (index_ordinal >= declaration->index_count ||
		declaration->parameter_count + declaration->index_count !=
			source_argument_count || source_argument_count != branch_argument_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
	}

	uint32_t parameter_substitution;
	if (prototype_context_substitution_from_terms(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			match_operation->context_id,
			declaration->parameter_context,
			source_arguments,
			declaration->parameter_count,
			&parameter_substitution
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index_classifier;
	if (prototype_context_telescope_entry_classifier(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			parameter_substitution,
			declaration->parameter_context,
			declaration->index_context,
			&source_arguments[declaration->parameter_count],
			index_ordinal,
			index_ordinal,
			&source_index_classifier
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index =
		source_arguments[declaration->parameter_count + index_ordinal];
	uint32_t branch_index;
	uint32_t branch_index_classifier;
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index,
			operation_case->refinement_substitution,
			&branch_index
		) != 0 || prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index_classifier,
			operation_case->refinement_substitution,
			&branch_index_classifier
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	int branch_index_matches;
	if (prototype_term_view_shape_equal(
			ctx->terms,
			branch_index,
			branch_arguments[declaration->parameter_count + index_ordinal],
			&branch_index_matches
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	if (!branch_index_matches) {
		/* The two terms are typed by the same nominal family index telescope.
		 * Core agreement is accepted only as validation of this already-derived
		 * correspondence; it is never used to find a replacement location. */
		if (prototype_term_core_shape_equal(
				ctx->terms,
				branch_index,
				branch_arguments[declaration->parameter_count + index_ordinal],
				&branch_index_matches
			) != 0) {
			return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
		}
		if (!branch_index_matches) {
			return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
		}
	}
	p_correspondence->family_type_id = family_type_id;
	p_correspondence->index_ordinal = index_ordinal;
	p_correspondence->source_index = source_index;
	p_correspondence->branch_index = branch_index;
	p_correspondence->source_index_classifier = source_index_classifier;
	p_correspondence->branch_index_classifier = branch_index_classifier;
	p_correspondence->branch_context = operation_case->context_id;
	p_correspondence->refinement_substitution =
		operation_case->refinement_substitution;
	return OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE;
}

static int operation_motive_abstract_index(
	struct compile_context* ctx,
	uint32_t body,
	uint32_t index_term,
	uint32_t replacement,
	uint32_t* p_result,
	int* p_dependency_found
) {
	if (!ctx || !p_result || !p_dependency_found ||
		body >= ctx->terms->term_count || index_term >= ctx->terms->term_count ||
		replacement >= ctx->terms->term_count) {
		return -1;
	}
	if (prototype_term_graph_replace_exact(
			ctx->terms,
			prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
			body,
			index_term,
			replacement,
			p_result
		) != 0) {
		return -1;
	}
	*p_dependency_found = *p_result != body;
	return 0;
}

static int operation_motive_subgraph_contains_occurrence_impl(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint32_t depth,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited || root_operation >=
			ctx->metadata->typed_occurrences.occurrence_count || target_operation >=
			ctx->metadata->typed_occurrences.occurrence_count || depth > 128) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[root_operation];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		int wrapped = operation_motive_subgraph_contains_occurrence_impl(
			ctx, operation->wrapped_occurrence, target_operation, depth + 1,
			visited
		);
		if (wrapped != 0) {
			return wrapped;
		}
	}
	if (operation->edge_count == 0) {
		return 0;
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID ||
		operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count > ctx->metadata->typed_occurrences.edge_count -
			operation->first_edge) {
		return -1;
	}
	for (uint32_t edge = 0; edge < operation->edge_count; ++edge) {
		uint32_t child = ctx->metadata->typed_occurrences.edges[
			operation->first_edge + edge
		].child_occurrence;
		int contains = operation_motive_subgraph_contains_occurrence_impl(
			ctx, child, target_operation, depth + 1, visited
		);
		if (contains != 0) {
			return contains;
		}
	}
	return 0;
}

static int operation_motive_subgraph_contains_occurrence(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	uint8_t visited[4096] = { 0 };
	return operation_motive_subgraph_contains_occurrence_impl(
		ctx, root_operation, target_operation, 0, visited
	);
}

static int operation_motive_validate_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t branch_operation,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !p_validation) {
		return -1;
	}
	for (uint32_t ih_operation = 0;
		ih_operation < ctx->metadata->typed_occurrences.occurrence_count;
		++ih_operation) {
		if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
			match_operation_id) {
			continue;
		}
		int contains = operation_motive_subgraph_contains_occurrence(
			ctx, branch_operation, ih_operation
		);
		if (contains < 0) {
			return -1;
		}
		if (contains == 0) {
			continue;
		}
		uint32_t argument_operation;
		const struct prototype_typed_occurrence* ih =
			&ctx->metadata->typed_occurrences.occurrences[ih_operation];
		if (ih->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			ih->ih_owner_occurrence != match_operation_id ||
			compile_occurrence_child(
				ctx, ih_operation, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
				&argument_operation
			) != 0) {
			return -1;
		}
		int guardedness = operation_solver_validate_guarded_motive_occurrence(
			ctx, ih_operation, match_operation_id, argument_operation
		);
		if (p_validation->ih_operation == PROTOTYPE_INVALID_ID) {
			p_validation->ih_operation = ih_operation;
			p_validation->guardedness_status = guardedness;
		}
		p_validation->used_recursive_equation = 1;
		if (guardedness != 0) {
			p_validation->guardedness_status = guardedness;
			return 1;
		}
	}
	return 0;
}

static int operation_motive_validate_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index,
	uint32_t motive,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !p_validation || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		motive >= ctx->terms->term_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	memset(p_validation, 0, sizeof(*p_validation));
	p_validation->expected_classifier = PROTOTYPE_INVALID_ID;
	p_validation->actual_classifier = PROTOTYPE_INVALID_ID;
	p_validation->ih_operation = PROTOTYPE_INVALID_ID;
	p_validation->conversion_status = PROTOTYPE_TERM_CONVERSION_INVALID;
	p_validation->guardedness_status = 1;

	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	const struct operation_constraint* equation =
		operation_solver_motive_equation(
			&ctx->constraints, match_operation_id, case_index
		);
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !equation ||
		equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t constructor;
	uint32_t constructor_classifier;
	int constructor_status = operation_solver_match_case_constructor_classifier(
		ctx,
		match_operation_id,
		case_index,
		&constructor,
		&constructor_classifier
	);
	if (constructor_status < 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	if (constructor_status > 0) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	uint32_t branch_operation =
		equation->payload.classifier.goal.motive_case.branch_operation;
	uint32_t actual = operation_solver_classifier(ctx, branch_operation);
	if (actual == PROTOTYPE_INVALID_ID) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	int recursive_validation = operation_motive_validate_recursive_equations(
		ctx, match_operation_id, branch_operation, p_validation
	);
	if (recursive_validation < 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	if (recursive_validation > 0) {
		return OPERATION_MOTIVE_EQUATION_MISMATCH;
	}
	if (p_validation->used_recursive_equation) {
		for (uint32_t ih_operation = 0;
			ih_operation < ctx->metadata->typed_occurrences.occurrence_count;
			++ih_operation) {
			if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
					match_operation_id) {
				continue;
			}
			int contains = operation_motive_subgraph_contains_occurrence(
				ctx, branch_operation, ih_operation
			);
			if (contains < 0) {
				return OPERATION_MOTIVE_EQUATION_INVALID;
			}
			if (contains > 0 &&
				(ctx->classifier_solver.ih_projected_motives[ih_operation] != motive ||
				 ctx->classifier_solver.classifier_metas[ih_operation].classifier ==
					PROTOTYPE_INVALID_ID)) {
				/* First project this candidate to every recursive premise, then
				 * recompute the branch through the ordinary dependency graph. */
				return OPERATION_MOTIVE_EQUATION_PENDING;
			}
		}
	}
	uint32_t branch_motive = motive;
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
		prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			motive,
			operation_case->refinement_substitution,
			&branch_motive
		) != 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t expected;
	uint32_t normalized_expected;
	if (prototype_judgement_apply_indexed_motive(
			ctx->terms,
			ctx->type_declarations,
			branch_motive,
			constructor,
			constructor_classifier,
			1,
			&expected
		) != 0 || prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			expected,
			&normalized_expected
		) != 0) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	p_validation->expected_classifier = normalized_expected;
	p_validation->actual_classifier = actual;
	p_validation->conversion_status = prototype_judgement_classifier_conversion(
		ctx->terms,
		ctx->type_declarations,
		normalized_expected,
		actual
	).status;
	if (p_validation->conversion_status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	/* Match motive equations constrain the branch result. The computation
	 * envelope is joined independently from all branch effect rows and
	 * totalities, so a value result and its returning-computation classifier
	 * denote the same motive equation without collapsing their CBPV categories. */
	struct prototype_term_classifier_view expected_view;
	struct prototype_term_classifier_view actual_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			normalized_expected,
			&expected_view
		) == 0 && prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			actual,
			&actual_view
		) == 0) {
		uint32_t expected_result = expected_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			expected_view.computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ?
				expected_view.result : normalized_expected;
		uint32_t actual_result = actual_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			actual_view.computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ?
				actual_view.result : actual;
		p_validation->conversion_status =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				expected_result,
				actual_result
			).status;
		if (p_validation->conversion_status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return OPERATION_MOTIVE_EQUATION_VALID;
		}
	}
	if (p_validation->used_recursive_equation &&
		p_validation->guardedness_status == 0) {
		/* Every recursive premise has already been projected from this exact
		 * motive and its downstream branch has been recomputed. The remaining
		 * difference is the guarded recursive equation itself, not an assertion
		 * of ordinary DefEq. */
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	return OPERATION_MOTIVE_EQUATION_MISMATCH;
}

static void operation_motive_clear_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id
) {
	if (!ctx) {
		return;
	}
	int changed = 0;
	for (uint32_t operation = 0;
		operation < ctx->metadata->typed_occurrences.occurrence_count;
		++operation) {
		if (ctx->classifier_solver.recursive_equation_owners[operation] ==
			match_operation_id) {
			ctx->classifier_solver.recursive_equation_owners[operation] =
				PROTOTYPE_INVALID_ID;
			changed = 1;
		}
	}
	if (changed) {
		ctx->classifier_solver.recursive_equation_revision++;
	}
}

static int operation_motive_has_recursive_equations(
	const struct compile_context* ctx,
	uint32_t match_operation_id
) {
	if (!ctx || !ctx->metadata || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t operation = 0;
		operation < ctx->metadata->typed_occurrences.occurrence_count;
		++operation) {
		if (ctx->classifier_solver.recursive_equation_owners[operation] ==
			match_operation_id) {
			return 1;
		}
	}
	return 0;
}

static int operation_motive_register_recursive_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t ih_operation
) {
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count || ih_operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[ih_operation].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		ctx->metadata->typed_occurrences.occurrences[ih_operation].
			ih_owner_occurrence != match_operation_id) {
		return -1;
	}
	if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
		match_operation_id) {
		ctx->classifier_solver.recursive_equation_owners[ih_operation] =
			match_operation_id;
		ctx->classifier_solver.recursive_equation_revision++;
	}
	return 0;
}

/* Diagnostics are emitted only after solving has stopped. Candidate metadata
 * is therefore a write-only explanation channel and never a solver input. */
static int operation_motive_add_terminal_diagnostic(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index
) {
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct operation_constraint* equation =
		operation_solver_motive_equation(
			&ctx->constraints, match_operation_id, case_index
		);
	if (!equation) {
		return -1;
	}
	const struct operation_motive_solution* solution =
		&ctx->classifier_solver.motive_solutions[match_operation_id];
	struct operation_motive_equation_validation validation = {
		.expected_classifier = PROTOTYPE_INVALID_ID,
		.actual_classifier = PROTOTYPE_INVALID_ID,
		.ih_operation = PROTOTYPE_INVALID_ID,
		.conversion_status = PROTOTYPE_TERM_CONVERSION_INVALID,
		.guardedness_status = 1
	};
	int validation_status = OPERATION_MOTIVE_EQUATION_PENDING;
	if (solution->motive < ctx->terms->term_count) {
		validation_status = operation_motive_validate_equation(
			ctx, match_operation_id, case_index, solution->motive, &validation
		);
	}
	if (validation_status == OPERATION_MOTIVE_EQUATION_VALID) {
		return 1;
	}
	int reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_EQUATION_MISMATCH;
	int abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE;
	uint64_t status_mask = 0;
	if (solution->conflict_mask != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_CORRESPONDENCE_CONFLICT;
		abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
		status_mask = solution->conflict_mask;
	} else if (solution->unavailable_mask != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
		abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
		status_mask = solution->unavailable_mask;
	} else if (validation_status == OPERATION_MOTIVE_EQUATION_MISMATCH &&
		validation.ih_operation != PROTOTYPE_INVALID_ID &&
		validation.guardedness_status != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_GUARDEDNESS;
	}
	uint32_t index_ordinal = PROTOTYPE_INVALID_ID;
	for (uint32_t index = 0; index < 64; ++index) {
		if ((status_mask & (UINT64_C(1) << index)) != 0) {
			index_ordinal = index;
			break;
		}
	}
	uint32_t branch_operation =
		equation->payload.classifier.goal.motive_case.branch_operation;
	const struct prototype_typed_occurrence* branch = branch_operation <
		ctx->metadata->typed_occurrences.occurrence_count ?
		&ctx->metadata->typed_occurrences.occurrences[branch_operation] : NULL;
	uint32_t source_ast = branch ? branch->source_ast : equation->source_ast;
	ctx->had_error = 1;
	return prototype_compile_metadata_add_diagnostic(
		ctx->metadata,
		(struct prototype_compile_diagnostic) {
			.phase = PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			.reason = reason,
			.source_ast = source_ast,
			.span = source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[source_ast].span :
				(struct prototype_source_span) { 0, 0 },
			.occurrence_id = branch_operation,
			.constraint_id = equation->id,
			.expected_classifier = validation.expected_classifier,
			.actual_classifier = validation.actual_classifier,
			.context_id = branch ? branch->context_id : PROTOTYPE_INVALID_ID,
			.constructor_ordinal =
				equation->payload.classifier.goal.motive_case.constructor_index,
			.field_ordinal = index_ordinal,
			.dependency_mask = solution->dependency_mask,
			.abstraction_status = abstraction_status,
			.guardedness_status = validation.guardedness_status,
			.conversion_status = validation.conversion_status
		}
	);
}

static int operation_solver_build_quoted_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	const struct prototype_typed_occurrence* operation,
	const struct prototype_term* match,
	const uint32_t* branch_results,
	const uint32_t* branch_constructor_terms,
	const uint32_t* branch_constructor_results,
	int returns_computation,
	uint32_t joined_effect_row,
	int joined_totality,
	uint32_t scrutinee_classifier,
	uint32_t scrutinee_type,
	const uint32_t* scrutinee_arguments,
	uint32_t scrutinee_argument_count,
	uint32_t scrutinee_parameter_count,
	uint32_t index_count,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !operation || !match || !branch_results ||
		!branch_constructor_terms || !branch_constructor_results ||
		!scrutinee_arguments || !p_classifier || index_count > 32 ||
		scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		scrutinee_argument_count != scrutinee_parameter_count + index_count) {
		return 1;
	}
	uint32_t common = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count;
		++case_index) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (!prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			return 1;
		}
		uint32_t quoted;
		int quote_status = operation_motive_quote_refined_term(
			ctx,
			branch_results[case_index],
			operation_case->refinement_substitution,
			&quoted
		);
		if (quote_status != 0) {
			return quote_status < 0 ? -1 : 1;
		}
		uint32_t quoted_constructor = branch_constructor_terms[case_index];
		uint32_t normalized_quoted;
		struct prototype_term_reduction_options normalization_options = {
			.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
		};
		if (prototype_term_nf_with_options(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				normalization_options,
				quoted,
				&normalized_quoted
			) != 0) {
			return -1;
		}
		quoted = normalized_quoted;
		uint32_t constructor_type;
		uint32_t constructor_arguments[64];
		uint32_t constructor_argument_count;
		if (prototype_term_type_instance_info(
				ctx->terms,
				branch_constructor_results[case_index],
				&constructor_type,
				constructor_arguments,
				&constructor_argument_count
			) != 0 || constructor_type != scrutinee_type ||
			constructor_argument_count != scrutinee_argument_count) {
			return 1;
		}
		for (uint32_t index = 0; index < index_count; ++index) {
			uint32_t quoted_index;
			int index_quote_status = operation_motive_quote_refined_term(
				ctx,
				constructor_arguments[scrutinee_parameter_count + index],
				operation_case->refinement_substitution,
				&quoted_index
			);
			uint32_t next_quoted;
			uint32_t next_constructor;
			if (index_quote_status != 0) {
				return index_quote_status < 0 ? -1 : 1;
			}
			if (prototype_term_graph_replace_exact(
					ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
					quoted,
					quoted_index,
					scrutinee_arguments[scrutinee_parameter_count + index],
					&next_quoted
				) != 0 || prototype_term_graph_replace_exact(
					ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
					quoted_constructor,
					constructor_arguments[scrutinee_parameter_count + index],
					scrutinee_arguments[scrutinee_parameter_count + index],
					&next_constructor
				) != 0) {
				return -1;
			}
			quoted = next_quoted;
			quoted_constructor = next_constructor;
		}
		uint32_t generalized;
		if (prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				quoted,
				quoted_constructor,
				match->as.match.scrutinee,
				&generalized
			) != 0) {
			return -1;
		}
		quoted = generalized;
		if (common == PROTOTYPE_INVALID_ID) {
			common = quoted;
		} else if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, common, quoted
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
	}
	uint32_t value_binding = prototype_term_new_binding(ctx->terms);
	uint32_t value_variable;
	uint32_t motive_body;
	if (common == PROTOTYPE_INVALID_ID || value_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, value_binding, &value_variable) != 0 ||
		prototype_term_graph_replace_exact(
			ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
			common,
			match->as.match.scrutinee,
			value_variable,
			&motive_body
		) != 0) {
		return -1;
	}
	uint32_t index_bindings[32];
	for (uint32_t index = 0; index < index_count; ++index) {
		uint32_t index_variable;
		uint32_t next;
		index_bindings[index] = prototype_term_new_binding(ctx->terms);
		if (index_bindings[index] == PROTOTYPE_INVALID_ID || prototype_term_var(
				ctx->terms, index_bindings[index], &index_variable
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				motive_body,
				scrutinee_arguments[scrutinee_parameter_count + index],
				index_variable,
				&next
			) != 0) {
			return -1;
		}
		motive_body = next;
	}
	if (returns_computation && prototype_term_computation_type(
			ctx->terms,
			joined_effect_row,
			motive_body,
			joined_totality,
			&motive_body
		) != 0) {
		return -1;
	}
	uint32_t motive;
	if (prototype_term_lambda(
			ctx->terms, value_binding, motive_body, &motive
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = index_count; reverse > 0; --reverse) {
		if (prototype_term_lambda(
				ctx->terms, index_bindings[reverse - 1], motive, &motive
			) != 0) {
			return -1;
		}
	}
	for (uint32_t case_index = 0; case_index < operation->case_count;
		++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		uint32_t expected_branch;
		uint32_t normalized_expected;
		if (!equation || prototype_judgement_apply_indexed_motive(
				ctx->terms,
				ctx->type_declarations,
				motive,
				branch_constructor_terms[case_index],
				branch_constructor_results[case_index],
				1,
				&expected_branch
			) != 0 || prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				expected_branch,
				&normalized_expected
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				normalized_expected,
				ctx->classifier_solver.classifier_metas[
					equation->payload.classifier.goal.motive_case.branch_operation
				].classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
	}
	uint32_t classifier = motive;
	for (uint32_t index = 0; index < index_count; ++index) {
		if (prototype_term_app(
				ctx->terms,
				classifier,
				scrutinee_arguments[scrutinee_parameter_count + index],
				&classifier
			) != 0) {
			return -1;
		}
	}
	return prototype_term_app(
		ctx->terms, classifier, match->as.match.scrutinee, p_classifier
	);
}

static int build_operation_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	int returns_computation = -1;
	uint32_t joined_effect_row = PROTOTYPE_INVALID_ID;
	int joined_totality = PROTOTYPE_COMPUTATION_TOTALITY_TOTAL;
	uint32_t branch_results[64];
	uint32_t branch_constructor_results[64];
	uint32_t branch_constructor_terms[64];
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[match->as.match.first_case + case_index];
		const struct prototype_typed_occurrence_match_case* source_operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
			source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256 ||
			source_operation_case->binder_count != source_case->binder_count) {
			return 1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.classifier_metas[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		int branch_is_refined = prototype_typed_occurrence_match_case_is_solved(source_operation_case);
		if (source_case->binder_count > 64) {
			return -1;
		}
		if (branch_is_refined) {
			reindexed_binder_count = source_operation_case->binder_count;
			motive_case_context = source_operation_case->context_id;
			for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
				reindexed_binders[i] = source_operation_case->binder_ids[i];
			}
			if (prototype_substitution_identity(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					motive_case_context,
					&case_substitution
				) != 0) {
				return -1;
			}
		} else if (prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0) {
			return -1;
		}
		if (reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			motive_binders[binder_cursor + i].binding_id =
				reindexed_binders[i];
			motive_binders[binder_cursor + i].is_recursive =
				ctx->terms->case_binders[
					source_case->first_binder + i
				].is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[match->as.match.first_case + case_index];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t reindexed_classifier;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				branch_classifier,
				case_substitution,
				&reindexed_classifier
			) != 0) {
			return -1;
		}
		int branch_returns;
		uint32_t branch_result;
		uint32_t branch_effect_row;
		int branch_totality;
		if (operation_solver_match_classifier_parts(
				ctx,
				reindexed_classifier,
				&branch_returns,
				&branch_result,
				&branch_effect_row,
				&branch_totality
			) != 0 || (returns_computation >= 0 &&
			branch_returns != returns_computation)) {
			return 1;
		}
		returns_computation = branch_returns;
		if (branch_returns) {
			joined_totality = prototype_computation_totality_join(
				joined_totality, branch_totality
			);
			if (joined_totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
				return 1;
			}
		}
		motive_cases[case_index].body = branch_result;
		branch_results[case_index] = branch_result;
		uint32_t constructor_term;
		uint32_t argument_classifiers[64];
		uint32_t constructor_classifier;
		int constructor_saturated;
		if (prototype_term_constructor(
				ctx->terms, source_case->constructor_owner,
				source_case->constructor_id, &constructor_term
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			uint32_t field;
			uint32_t binding_context;
			const struct prototype_context* binding_entry;
			if (prototype_term_var(
					ctx->terms,
					source_operation_case->binder_ids[i],
					&field
				) != 0 || prototype_term_app(
					ctx->terms, constructor_term, field, &constructor_term
				) != 0 || prototype_context_find_binding(
					&ctx->metadata->contexts,
					ctx->metadata->typed_occurrences.cases[
						operation->first_case + case_index
					].context_id,
					source_operation_case->binder_ids[i],
					&binding_context
				) != 0 || !(binding_entry = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				))) {
				return -1;
			}
			argument_classifiers[i] = prototype_context_classifier_term(binding_entry);
		}
		if (prototype_judgement_constructor_spine_classifier(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				constructor_term, source_case->constructor_owner,
				argument_classifiers, source_case->binder_count,
				&constructor_classifier, &constructor_saturated
			) != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
			!constructor_saturated || prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_term, case_substitution,
				&branch_constructor_terms[case_index]
			) != 0 || prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_classifier, case_substitution,
				&branch_constructor_results[case_index]
			) != 0) {
			return -1;
		}
		if (branch_returns) {
			if (joined_effect_row == PROTOTYPE_INVALID_ID) {
				joined_effect_row = branch_effect_row;
			} else if (prototype_term_effect_row_union(
					ctx->terms,
					joined_effect_row,
					branch_effect_row,
					&joined_effect_row
				) != 0) {
				return -1;
			}
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t scrutinee_operation;
	if (compile_occurrence_child(
			ctx, typing->operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0) {
		return -1;
	}
	uint32_t scrutinee_classifier = operation_solver_classifier(
		ctx, scrutinee_operation
	);
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	int has_indexed_scrutinee = scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) == 0 && scrutinee_type < ctx->type_declarations->semantic_schema.type_count;
	uint32_t index_count = has_indexed_scrutinee ?
		ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type].index_count : 0;
	uint32_t scrutinee_parameter_count = has_indexed_scrutinee ?
		ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type].parameter_count : 0;
	/* An indexed motive synthesized by a recursive-use equation becomes
	 * authoritative only after applying it to every constructor equation recovers
	 * that branch's independently synthesized classifier. This handles families
	 * such as Vec append, where the varying index occurs below Pi and cannot be
	 * recovered by comparing only the branch result roots. */
	uint32_t provisional_motive =
		ctx->classifier_solver.motive_solutions[typing->operation].motive;
	uint32_t motive_lambda_count = 0;
	uint32_t motive_cursor = provisional_motive;
	while (motive_cursor < ctx->terms->term_count &&
		ctx->terms->terms[motive_cursor].tag == PROTOTYPE_TERM_LAMBDA &&
		motive_lambda_count <= index_count) {
		motive_lambda_count++;
		motive_cursor = ctx->terms->terms[motive_cursor].as.lambda.body;
	}
	int validated_indexed_motive = index_count > 0 &&
		motive_lambda_count == index_count + 1;
	for (uint32_t case_index = 0;
		validated_indexed_motive && case_index < operation->case_count;
		++case_index) {
		struct operation_motive_equation_validation validation;
		int validation_status = operation_motive_validate_equation(
			ctx,
			typing->operation,
			case_index,
			provisional_motive,
			&validation
		);
		if (validation_status == OPERATION_MOTIVE_EQUATION_INVALID) {
			return -1;
		}
		if (validation_status != OPERATION_MOTIVE_EQUATION_VALID &&
			operation_motive_has_recursive_equations(
				ctx, typing->operation
			) > 0) {
			/* Recursive candidates are retained until their exact IH projections
			 * and downstream branch classifiers stabilize. A unary fallback here
			 * would erase the equation before it can be checked. */
			return 1;
		}
		validated_indexed_motive =
			validation_status == OPERATION_MOTIVE_EQUATION_VALID;
	}
	if (validated_indexed_motive) {
		uint32_t classifier = provisional_motive;
		for (uint32_t index = 0; index < index_count; ++index) {
			if (prototype_term_app(
					ctx->terms, classifier,
					scrutinee_arguments[scrutinee_parameter_count + index],
					&classifier
				) != 0) {
				return -1;
			}
		}
		return prototype_term_app(
			ctx->terms, classifier, match->as.match.scrutinee, p_classifier
		);
	}
	/* Quoting branch classifiers is a fallback for indexed motives that cannot
	 * be reconstructed from the explicit recursive-use equations. Prefer the
	 * validated index-and-value motive above: a unary quoted Match does not carry
	 * enough information to replay an IH at a recursive field's index. */
	if (has_indexed_scrutinee) {
		int quoted_status = operation_solver_build_quoted_motive(
			ctx,
			typing,
			operation,
			match,
			branch_results,
			branch_constructor_terms,
			branch_constructor_results,
			returns_computation,
			joined_effect_row,
			joined_totality,
			scrutinee_classifier,
			scrutinee_type,
			scrutinee_arguments,
			scrutinee_argument_count,
			scrutinee_parameter_count,
			index_count,
			p_classifier
		);
		if (quoted_status <= 0) {
			return quoted_status;
		}
	}
	/* Infer an ordinary dependent motive by abstracting each branch's saturated
	 * constructor value from its result classifier. This covers motives such as
	 * `\n => LE n n` and `\xs => Result (length xs)` without constraining the
	 * result family to have the scrutinee family's index arity. Reinstantiation
	 * is checked before the inferred motive is accepted. */
	uint32_t motive_binding = prototype_term_new_binding(ctx->terms);
	uint32_t motive_var;
	uint32_t generalized_result = PROTOTYPE_INVALID_ID;
	/* An indexed single-constructor eliminator may depend on both the family
	 * index and the scrutinee proof.  Its motive must abstract both arguments;
	 * the unary generalization below can abstract only the scrutinee. */
	int generalized_motive = !(has_indexed_scrutinee && index_count > 0 &&
		operation->case_count == 1) && motive_binding != PROTOTYPE_INVALID_ID &&
		prototype_term_var(ctx->terms, motive_binding, &motive_var) == 0;
	for (uint32_t i = 0; generalized_motive && i < operation->case_count; ++i) {
		uint32_t generalized_branch;
		uint32_t reinstantiated_branch;
		if (prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), branch_results[i],
				branch_constructor_terms[i], motive_var, &generalized_branch
			) != 0 || generalized_branch == branch_results[i] ||
			prototype_term_graph_substitute_bound_var(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), generalized_branch,
				motive_binding, branch_constructor_terms[i],
				&reinstantiated_branch
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				reinstantiated_branch, branch_results[i]
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			generalized_motive = 0;
			break;
		}
		if (generalized_result == PROTOTYPE_INVALID_ID) {
			generalized_result = generalized_branch;
		} else if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				generalized_result, generalized_branch
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			generalized_motive = 0;
		}
	}
	if (generalized_motive && generalized_result != PROTOTYPE_INVALID_ID) {
		uint32_t motive_body = generalized_result;
		uint32_t motive;
		if (returns_computation && prototype_term_computation_type(
				ctx->terms, joined_effect_row, generalized_result, joined_totality,
				&motive_body
			) != 0) {
			return -1;
		}
		if (prototype_term_lambda(
				ctx->terms, motive_binding, motive_body, &motive
			) != 0 || prototype_term_app(
				ctx->terms, motive, match->as.match.scrutinee, p_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	/* A result family may place a scrutinee index in either a declared parameter
	 * or index position. Infer the dependency from branch equations themselves:
	 * each varying result argument must coincide with one scrutinee-constructor
	 * index in every branch. */
	if (has_indexed_scrutinee && index_count > 0) {
		uint32_t result_type = PROTOTYPE_INVALID_ID;
		uint32_t result_argument_count = 0;
		uint32_t branch_result_arguments[64][32];
		uint32_t branch_constructor_arguments[64][32];
		int result_instance_motive = operation->case_count != 0;
		for (uint32_t branch = 0;
			result_instance_motive && branch < operation->case_count; ++branch) {
			uint32_t branch_result_type;
			uint32_t branch_result_count;
			uint32_t constructor_type;
			uint32_t constructor_count;
			if (prototype_term_type_instance_info(
					ctx->terms, branch_results[branch], &branch_result_type,
					branch_result_arguments[branch], &branch_result_count
				) != 0 || prototype_term_type_instance_info(
					ctx->terms, branch_constructor_results[branch], &constructor_type,
					branch_constructor_arguments[branch], &constructor_count
				) != 0 || constructor_type != scrutinee_type ||
				constructor_count != scrutinee_argument_count ||
				branch_result_count > 32 || (branch != 0 &&
				 (branch_result_type != result_type ||
				  branch_result_count != result_argument_count))) {
				result_instance_motive = 0;
				break;
			}
			result_type = branch_result_type;
			result_argument_count = branch_result_count;
		}
		uint32_t index_bindings[32];
		uint32_t index_variables[32];
		for (uint32_t index = 0;
			result_instance_motive && index < index_count; ++index) {
			index_bindings[index] = prototype_term_new_binding(ctx->terms);
			if (index_bindings[index] == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, index_bindings[index], &index_variables[index]
				) != 0) {
				return -1;
			}
		}
		uint32_t generic_result_arguments[32];
		for (uint32_t argument = 0;
			result_instance_motive && argument < result_argument_count; ++argument) {
			int constant = 1;
			for (uint32_t branch = 0;
				constant && branch < operation->case_count; ++branch) {
				for (uint32_t binder = 0;
					binder < motive_cases[branch].binder_count; ++binder) {
					if (prototype_term_contains_free_binding(
							ctx->terms,
							branch_result_arguments[branch][argument],
							motive_cases[branch].binders[binder].binding_id
						)) {
						constant = 0;
						break;
					}
				}
			}
			for (uint32_t branch = 1; branch < operation->case_count; ++branch) {
				if (prototype_judgement_classifier_conversion(
						ctx->terms, ctx->type_declarations,
						branch_result_arguments[0][argument],
						branch_result_arguments[branch][argument]
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					constant = 0;
					break;
				}
			}
			if (constant) {
				generic_result_arguments[argument] =
					branch_result_arguments[0][argument];
				continue;
			}
			uint32_t matching_index = PROTOTYPE_INVALID_ID;
			for (uint32_t index = 0; index < index_count; ++index) {
				int matches = 1;
				for (uint32_t branch = 0;
					branch < operation->case_count; ++branch) {
					if (prototype_judgement_classifier_conversion(
							ctx->terms, ctx->type_declarations,
							branch_result_arguments[branch][argument],
							branch_constructor_arguments[branch][
								scrutinee_parameter_count + index
							]
						).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
						matches = 0;
						break;
					}
				}
				if (matches) {
					matching_index = index;
					break;
				}
			}
			if (matching_index == PROTOTYPE_INVALID_ID) {
				result_instance_motive = 0;
				break;
			}
			generic_result_arguments[argument] = index_variables[matching_index];
		}
		if (result_instance_motive) {
			uint32_t generic_result;
			uint32_t motive_body;
			uint32_t value_binding = prototype_term_new_binding(ctx->terms);
			uint32_t motive;
			if (value_binding == PROTOTYPE_INVALID_ID ||
				prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, result_type,
					generic_result_arguments, result_argument_count,
					&generic_result
				) != 0) {
				return -1;
			}
			motive_body = generic_result;
			if (returns_computation && prototype_term_computation_type(
					ctx->terms, joined_effect_row, generic_result, joined_totality,
					&motive_body
				) != 0) {
				return -1;
			}
			if (prototype_term_lambda(
					ctx->terms, value_binding, motive_body, &motive
				) != 0) {
				return -1;
			}
			for (uint32_t index = index_count; index > 0; --index) {
				if (prototype_term_lambda(
						ctx->terms, index_bindings[index - 1], motive, &motive
					) != 0) {
					return -1;
				}
			}
			uint32_t classifier = motive;
			for (uint32_t index = 0; index < index_count; ++index) {
				if (prototype_term_app(
						ctx->terms, classifier,
						scrutinee_arguments[scrutinee_parameter_count + index],
						&classifier
					) != 0) {
					return -1;
				}
			}
			return prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			);
		}
	}

	/* Indexed motives are ordinary curried lambdas in TermDB:
	 * M : i1 -> ... -> in -> scrutinee -> classifier.  Infer this form when
	 * every branch returns one common indexed family and differs only in its
	 * index spine.  This is synthesized from branch classifiers and the
	 * scrutinee family; no expected `::` type participates. */
	uint32_t result_type = PROTOTYPE_INVALID_ID;
	uint32_t result_arguments[64][32];
	uint32_t result_argument_counts[64];
	int indexed_motive = has_indexed_scrutinee;
	for (uint32_t i = 0; indexed_motive && i < operation->case_count; ++i) {
		uint32_t branch_type;
		uint32_t constructor_type;
		uint32_t constructor_arguments[32];
		uint32_t constructor_argument_count;
		if (prototype_term_type_instance_info(
				ctx->terms, branch_results[i], &branch_type,
				result_arguments[i], &result_argument_counts[i]
			) != 0 || prototype_term_type_instance_info(
				ctx->terms, branch_constructor_results[i], &constructor_type,
				constructor_arguments, &constructor_argument_count
			) != 0 || constructor_type != scrutinee_type ||
			constructor_argument_count != scrutinee_argument_count ||
			branch_type >= ctx->type_declarations->semantic_schema.type_count ||
			(i != 0 && (branch_type != result_type ||
			 result_argument_counts[i] != result_argument_counts[0]))) {
			indexed_motive = 0;
			break;
		}
		uint32_t branch_parameter_count =
			ctx->type_declarations->semantic_schema.type_declarations[branch_type].parameter_count;
		for (uint32_t index = 0; indexed_motive && index < index_count; ++index) {
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					result_arguments[i][branch_parameter_count + index],
					constructor_arguments[scrutinee_parameter_count + index]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				indexed_motive = 0;
			}
		}
		result_type = branch_type;
	}
	uint32_t result_parameter_count = indexed_motive ?
		ctx->type_declarations->semantic_schema.type_declarations[result_type].parameter_count : 0;
	if (indexed_motive &&
		(ctx->type_declarations->semantic_schema.type_declarations[result_type].index_count !=
			index_count || result_argument_counts[0] !=
				result_parameter_count + index_count || index_count == 0)) {
		indexed_motive = 0;
	}
	for (uint32_t i = 1; indexed_motive && i < operation->case_count; ++i) {
		for (uint32_t p = 0; p < result_parameter_count; ++p) {
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					result_arguments[0][p], result_arguments[i][p]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				indexed_motive = 0;
				break;
			}
		}
	}
	/* A result family such as Acc's eliminator returns P(index), not another
	 * indexed ADT instance.  For a single constructor whose result indices are
	 * distinct field variables, abstraction is exact: substitute those field
	 * bindings by fresh motive index binders. More general constructor index
	 * expressions remain with the branch-Match motive below. */
	if (!indexed_motive && scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		operation->case_count == 1 && scrutinee_type <
			ctx->type_declarations->semantic_schema.type_count && index_count > 0 &&
		prototype_term_type_instance_info(
			ctx->terms, branch_constructor_results[0], &(uint32_t){0},
			scrutinee_arguments, &scrutinee_argument_count
		) == 0) {
		uint32_t parameter_count = ctx->type_declarations->semantic_schema.type_declarations[
			scrutinee_type
		].parameter_count;
		uint32_t value_binding = prototype_term_new_binding(ctx->terms);
		uint32_t value_var;
		uint32_t generalized;
		uint32_t index_bindings[32];
		uint32_t index_vars[32];
		uint32_t pi_domains[64];
		uint32_t pi_bindings[64];
		uint32_t pi_count = 0;
		int can_abstract = index_count <= 32 &&
			scrutinee_argument_count == parameter_count + index_count &&
			value_binding != PROTOTYPE_INVALID_ID &&
			prototype_term_var(ctx->terms, value_binding, &value_var) == 0;
		uint32_t leaf = branch_results[0];
		while (can_abstract && leaf < ctx->terms->term_count &&
			ctx->terms->terms[leaf].tag == PROTOTYPE_TERM_PI) {
			if (pi_count >= 64) {
				can_abstract = 0;
				break;
			}
			pi_domains[pi_count] = ctx->terms->terms[leaf].as.pi.domain;
			if (prototype_term_pure_family_parts(
					ctx->terms, ctx->terms->terms[leaf].as.pi.codomain_family,
					&pi_bindings[pi_count], &leaf
				) != 0) {
				can_abstract = 0;
				break;
			}
			pi_count++;
		}
		uint32_t normalized_leaf;
		if (can_abstract && (prototype_term_normalize_complete_with_profile(
				ctx->terms, ctx->type_declarations, NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				leaf, &normalized_leaf
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), normalized_leaf,
				branch_constructor_terms[0], value_var, &generalized
			) != 0)) {
			can_abstract = 0;
		}
		for (uint32_t i = pi_count; can_abstract && i > 0; --i) {
			uint32_t family;
			if (prototype_term_pure_family(
					ctx->terms, pi_bindings[i - 1], generalized, &family
				) != 0 || prototype_term_pi_family(
					ctx->terms, pi_domains[i - 1], family, &generalized
				) != 0) {
				can_abstract = 0;
			}
		}
		for (uint32_t i = 0; can_abstract && i < index_count; ++i) {
			uint32_t source_index = scrutinee_arguments[parameter_count + i];
			index_bindings[i] = prototype_term_new_binding(ctx->terms);
			if (source_index >= ctx->terms->term_count ||
				ctx->terms->terms[source_index].tag != PROTOTYPE_TERM_VAR ||
				index_bindings[i] == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, index_bindings[i], &index_vars[i]
				) != 0 || prototype_term_graph_substitute_bound_var(
					ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), generalized,
					ctx->terms->terms[source_index].as.var.binding_id,
					index_vars[i], &generalized
				) != 0) {
				can_abstract = 0;
			}
		}
		if (can_abstract) {
			uint32_t motive_body = generalized;
			uint32_t motive;
			if (returns_computation && prototype_term_computation_type(
					ctx->terms, joined_effect_row, generalized, joined_totality,
					&motive_body
				) != 0) {
				return -1;
			}
			if (prototype_term_lambda(
					ctx->terms, value_binding, motive_body, &motive
				) != 0) {
				return -1;
			}
			for (uint32_t i = index_count; i > 0; --i) {
				if (prototype_term_lambda(
						ctx->terms, index_bindings[i - 1], motive, &motive
					) != 0) {
					return -1;
				}
			}
			uint32_t classifier = motive;
			uint32_t source_type;
			uint32_t source_arguments[32];
			uint32_t source_argument_count;
			if (prototype_term_type_instance_info(
					ctx->terms, scrutinee_classifier, &source_type,
					source_arguments, &source_argument_count
				) != 0 || source_type != scrutinee_type ||
				source_argument_count != parameter_count + index_count) {
				return -1;
			}
			for (uint32_t i = 0; i < index_count; ++i) {
				if (prototype_term_app(
						ctx->terms, classifier,
						source_arguments[parameter_count + i], &classifier
					) != 0) {
					return -1;
				}
			}
			return prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			);
		}
	}
	if (indexed_motive) {
		uint32_t index_bindings[32];
		uint32_t generic_arguments[32];
		for (uint32_t p = 0; p < result_parameter_count; ++p) {
			generic_arguments[p] = result_arguments[0][p];
		}
		for (uint32_t i = 0; i < index_count; ++i) {
			index_bindings[i] = prototype_term_new_binding(ctx->terms);
			if (index_bindings[i] == PROTOTYPE_INVALID_ID ||
				prototype_term_var(
					ctx->terms, index_bindings[i],
					&generic_arguments[result_parameter_count + i]
				) != 0) {
				return -1;
			}
		}
		uint32_t generic_result;
		uint32_t motive_body;
		uint32_t value_binding = prototype_term_new_binding(ctx->terms);
		uint32_t motive;
		if (value_binding == PROTOTYPE_INVALID_ID ||
			prototype_term_type_instance_make(
				ctx->terms, ctx->type_declarations, result_type,
				generic_arguments, result_parameter_count + index_count,
				&generic_result
			) != 0) {
			return -1;
		}
		motive_body = generic_result;
		if (returns_computation && prototype_term_computation_type(
				ctx->terms, joined_effect_row, generic_result, joined_totality,
				&motive_body
			) != 0) {
			return -1;
		}
		if (prototype_term_lambda(
				ctx->terms, value_binding, motive_body, &motive
			) != 0) {
			return -1;
		}
		for (uint32_t i = index_count; i > 0; --i) {
			if (prototype_term_lambda(
					ctx->terms, index_bindings[i - 1], motive, &motive
				) != 0) {
				return -1;
			}
		}
		uint32_t classifier = motive;
		uint32_t scrutinee_parameter_count =
			ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type].parameter_count;
		for (uint32_t i = 0; i < index_count; ++i) {
			if (prototype_term_app(
					ctx->terms, classifier,
					scrutinee_arguments[scrutinee_parameter_count + i],
					&classifier
				) != 0) {
				return -1;
			}
		}
		if (prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee,
				p_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t binder_var;
	uint32_t motive_match;
	uint32_t motive_body;
	uint32_t motive;
	uint32_t index_bindings[32];
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_match(ctx->terms, binder_var, motive_cases, operation->case_count, &motive_match) != 0) {
		return -1;
	}
	motive_body = motive_match;
	if (returns_computation && prototype_term_computation_type(
			ctx->terms, joined_effect_row, motive_match, joined_totality,
			&motive_body
		) != 0) {
		return -1;
	}
	if (prototype_term_lambda(ctx->terms, binding_id, motive_body, &motive) != 0) {
		return -1;
	}
	/* Even when the branch Match alone determines the result, an indexed
	 * eliminator's motive has one argument per family index before its value
	 * argument. Preserve that arity so IH replay can form M(index, recursive)
	 * without treating a unary fallback as an indexed family. */
	for (uint32_t index = 0; index < index_count; ++index) {
		index_bindings[index] = prototype_term_new_binding(ctx->terms);
		if (index_bindings[index] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (uint32_t reverse = index_count; reverse > 0; --reverse) {
		if (prototype_term_lambda(
				ctx->terms, index_bindings[reverse - 1], motive, &motive
			) != 0) {
			return -1;
		}
	}
	uint32_t classifier = motive;
	for (uint32_t index = 0; index < index_count; ++index) {
		if (prototype_term_app(
				ctx->terms, classifier,
				scrutinee_arguments[scrutinee_parameter_count + index],
				&classifier
			) != 0) {
			return -1;
		}
	}
	return prototype_term_app(
		ctx->terms, classifier, match->as.match.scrutinee, p_classifier
	);
}


static int build_operation_uniform_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.classifier_metas[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t branch_classifier_whnf;
		int capture_status = operation_classifier_captures_case_binder(
			ctx, equation, branch_classifier, &branch_classifier_whnf
		);
		if (capture_status < 0) {
			return -1;
		}
		if (capture_status > 0) {
			return 1;
		}
		if (classifier == PROTOTYPE_INVALID_ID) {
			classifier = branch_classifier_whnf;
		} else {
			uint32_t joined;
			int join_status = operation_solver_join_match_classifiers(
				ctx, classifier, branch_classifier_whnf, &joined
			);
			if (join_status != 0) {
				return join_status;
			}
			classifier = joined;
		}
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_lambda(ctx->terms, binding_id, classifier, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}


static int build_operation_guarded_recursive_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	uint32_t motive_frame = prototype_term_new_ih_scope(ctx->terms);
	if (motive_frame == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		uint32_t source_case_id = match->as.match.first_case + case_index;
		if (!equation || source_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[source_case_id];
		const struct prototype_typed_occurrence_match_case* source_operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256 ||
			source_operation_case->binder_count != source_case->binder_count) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64) {
			return -1;
		}
		if (prototype_typed_occurrence_match_case_is_solved(source_operation_case)) {
			reindexed_binder_count = source_operation_case->binder_count;
			motive_case_context = source_operation_case->context_id;
			for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
				reindexed_binders[i] = source_operation_case->binder_ids[i];
			}
			if (prototype_substitution_identity(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					motive_case_context,
					&case_substitution
				) != 0) {
				return -1;
			}
		} else if (prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0) {
			return -1;
		}
		if (reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t binder_index = 0;
			binder_index < source_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* source_binder =
				&ctx->terms->case_binders[source_case->first_binder + binder_index];
			motive_binders[binder_cursor + binder_index].binding_id =
				reindexed_binders[binder_index];
			motive_binders[binder_cursor + binder_index].is_recursive =
				source_binder->is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[source_case_id];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t branch_classifier =
			ctx->classifier_solver.classifier_metas[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier != PROTOTYPE_INVALID_ID) {
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					branch_classifier,
					case_substitution,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		} else {
			const struct prototype_typed_occurrence* branch =
				&ctx->metadata->typed_occurrences.occurrences[
					equation->payload.classifier.goal.motive_case.branch_operation
				];
			uint32_t branch_argument;
			if (branch->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
				branch->ih_owner_occurrence != typing->operation ||
				compile_occurrence_child(
					ctx, equation->payload.classifier.goal.motive_case.branch_operation,
					PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
					&branch_argument
				) != 0) {
				return 1;
			}
			uint32_t original_argument =
				ctx->metadata->typed_occurrences.occurrences[branch_argument].core_term;
			if (original_argument >= ctx->terms->term_count ||
				ctx->terms->terms[original_argument].tag != PROTOTYPE_TERM_VAR) {
				return 1;
			}
			uint32_t recursive_index = PROTOTYPE_INVALID_ID;
			for (uint32_t binder_index = 0;
				binder_index < source_case->binder_count;
				++binder_index) {
				const struct prototype_case_binder* source_binder =
					&ctx->terms->case_binders[source_case->first_binder + binder_index];
				if (source_binder->is_recursive &&
					source_operation_case->binder_ids[binder_index] ==
						ctx->terms->terms[original_argument].as.var.binding_id) {
					recursive_index = binder_index;
					break;
				}
			}
			if (recursive_index == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			uint32_t recursive_var;
			if (prototype_term_var(
					ctx->terms,
					motive_binders[binder_cursor + recursive_index].binding_id,
					&recursive_var
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, motive_frame, recursive_var,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
	uint32_t motive_var;
	uint32_t motive_match;
	uint32_t motive;
	if (motive_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, motive_binder, &motive_var) != 0 ||
		prototype_term_match_with_ih_scope(
			ctx->terms, motive_var, motive_cases, operation->case_count,
			motive_frame, &motive_match
		) != 0 ||
		prototype_term_set_ih_scope_term(ctx->terms, motive_frame, motive_match) != 0) {
		return -1;
	}

	uint32_t scrutinee_operation;
	uint32_t scrutinee_classifier = PROTOTYPE_INVALID_ID;
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count = 0;
	uint32_t parameter_count = 0;
	uint32_t index_count = 0;
	if (compile_occurrence_child(
			ctx, typing->operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) == 0) {
		scrutinee_classifier = operation_solver_classifier(
			ctx, scrutinee_operation
		);
	}
	if (scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) == 0 && scrutinee_type <
			ctx->type_declarations->semantic_schema.type_count) {
		const struct prototype_type_declaration* declaration =
			&ctx->type_declarations->semantic_schema.type_declarations[
				scrutinee_type
			];
		parameter_count = declaration->parameter_count;
		index_count = declaration->index_count;
		if (index_count > 32 || scrutinee_argument_count !=
				parameter_count + index_count) {
			return -1;
		}
	}

	uint32_t index_bindings[32];
	for (uint32_t index = 0; index < index_count; ++index) {
		uint32_t index_variable;
		uint32_t generalized_match;
		index_bindings[index] = prototype_term_new_binding(ctx->terms);
		if (index_bindings[index] == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, index_bindings[index], &index_variable
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				motive_match,
				scrutinee_arguments[parameter_count + index],
				index_variable,
				&generalized_match
			) != 0) {
			return -1;
		}
		motive_match = generalized_match;
	}
	if (prototype_term_lambda(
			ctx->terms, motive_binder, motive_match, &motive
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = index_count; reverse > 0; --reverse) {
		if (prototype_term_lambda(
				ctx->terms, index_bindings[reverse - 1], motive, &motive
			) != 0) {
			return -1;
		}
	}
	uint32_t classifier = motive;
	for (uint32_t index = 0; index < index_count; ++index) {
		if (prototype_term_app(
				ctx->terms, classifier,
				scrutinee_arguments[parameter_count + index], &classifier
			) != 0) {
			return -1;
		}
	}
	return prototype_term_app(
		ctx->terms, classifier, match->as.match.scrutinee, p_classifier
	);
}

/*
 * This is the solver occurs check for the only recursive metavariable form
 * currently admitted by the language: an IH denotes M(rest). The occurrence
 * must point back to its own Match frame and `rest` must be a recursive field
 * of that Match. A raw M(x) self-reference outside that structural edge is
 * rejected instead of becoming a cyclic solver substitution.
 */

static int operation_solver_materialize_match_solution(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	uint8_t state = ctx->classifier_solver.motive_solutions[operation_id].state;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !typing ||
		state == OPERATION_MOTIVE_SOLUTION_UNRESOLVED ||
		state == OPERATION_MOTIVE_SOLUTION_PROVISIONAL ||
		state == OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int all_branches_impossible = operation->case_count > 0;
	for (uint32_t case_index = 0;
		case_index < operation->case_count;
		++case_index) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (operation_case->refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			all_branches_impossible = 0;
			break;
		}
	}
	if (all_branches_impossible &&
		ctx->classifier_solver.classifier_metas[operation_id].classifier !=
			PROTOTYPE_INVALID_ID) {
		uint32_t existing =
			ctx->classifier_solver.classifier_metas[operation_id].classifier;
		if (existing >= ctx->terms->term_count ||
			ctx->terms->terms[existing].tag != PROTOTYPE_TERM_APP) {
			return -1;
		}
		uint32_t motive = ctx->terms->terms[existing].as.app.function;
		if (motive >= ctx->terms->term_count ||
			ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		/* Empty elimination has no reachable branch equation.  Its result
		 * classifier must therefore come from the surrounding constraint, not
		 * from the syntactic bodies of impossible branches. */
		ctx->classifier_solver.motive_solutions[operation_id].motive = motive;
		ctx->classifier_solver.motive_solutions[operation_id].state =
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		return operation_solver_enqueue_dependents(ctx, operation_id);
	}
	uint32_t classifier;
	int status;
	if (state == OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE) {
		status = build_operation_guarded_recursive_motive(ctx, typing, &classifier);
	} else if (operation_motive_has_recursive_equations(ctx, operation_id) > 0 &&
		ctx->classifier_solver.motive_solutions[operation_id].motive <
			ctx->terms->term_count) {
		/* A motive reconstructed from an indexed or guarded recursive equation is
		 * the authority. Branch classifiers are consequences of that equation and
		 * may still carry an older provisional constant while the fixed point is
		 * propagating. Validate the authoritative motive against every branch before
		 * considering a uniform fallback; rebuilding from those consequences would
		 * reverse the dependency and can erase a valid dependent index. */
		status = build_operation_motive(ctx, typing, &classifier);
	} else {
		status = build_operation_uniform_motive(ctx, typing, &classifier);
		if (status > 0) {
			status = build_operation_motive(ctx, typing, &classifier);
		}
	}
	if (status != 0) {
		return status < 0 ? -1 : 0;
	}
	/* The constant candidate is a bootstrap equation from one non-recursive
	 * branch, not a proposed classifier for the complete Match.  In a dependent
	 * motive the complete classifier need not be convertible to that branch
	 * equation; build_operation_motive has already checked every equation. */
	uint32_t motive = ctx->terms->terms[classifier].as.app.function;
	while (motive < ctx->terms->term_count &&
		ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
		motive = ctx->terms->terms[motive].as.app.function;
	}
	if (motive >= ctx->terms->term_count ||
		ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	ctx->classifier_solver.motive_solutions[operation_id].motive = motive;
	ctx->classifier_solver.motive_solutions[operation_id].state =
		OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
	int bind_status = operation_solver_bind(ctx, operation_id, classifier, p_changed);
	if (bind_status != 0) {
		return bind_status;
	}
	/* Motive materialization is solver state that IH constraints consume.  The
	 * Match classifier may already have been provisionally bound to the same
	 * Term, so classifier binding alone is not guaranteed to wake them. */
	return operation_solver_enqueue_dependents(ctx, operation_id);
}

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		if (ctx->metadata->typed_occurrences.occurrences[i].tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			operation_solver_materialize_match_solution(ctx, i, p_changed) != 0) {
			fprintf(stderr,
				"classifier solver motive materialization failed operation=%u\n",
				i);
			return -1;
		}
	}
	return 0;
}

struct operation_match_motive_rebuild_transaction {
	uint32_t operation;
	struct operation_classifier_meta_solution classifier_meta;
	struct operation_motive_solution motive_solution;
	uint64_t solution_revision;
	int active;
};

static int operation_match_motive_rebuild_transaction_begin(
	struct compile_context* ctx,
	uint32_t operation,
	struct operation_match_motive_rebuild_transaction* transaction
) {
	if (!ctx || !transaction || operation >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[operation];
	if (meta->owner_occurrence != operation ||
		meta->state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	*transaction = (struct operation_match_motive_rebuild_transaction){
		.operation = operation,
		.classifier_meta = *meta,
		.motive_solution = ctx->classifier_solver.motive_solutions[operation],
		.solution_revision = ctx->classifier_solver.solution_revision,
		.active = 1
	};
	if (meta->classifier != PROTOTYPE_INVALID_ID) {
		ctx->classifier_solver.solution_revision++;
	}
	meta->classifier = PROTOTYPE_INVALID_ID;
	meta->evidence_constraint_id = PROTOTYPE_INVALID_ID;
	meta->state = OPERATION_CLASSIFIER_META_UNSOLVED;
	ctx->classifier_solver.motive_solutions[operation].constant_candidate =
		PROTOTYPE_INVALID_ID;
	ctx->classifier_solver.motive_solutions[operation].state =
		OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY;
	ctx->classifier_seed_suppressed[operation] = 1;
	return 0;
}

static int operation_match_motive_rebuild_transaction_rollback(
	struct compile_context* ctx,
	struct operation_match_motive_rebuild_transaction* transaction
) {
	if (!ctx || !transaction || !transaction->active ||
		transaction->operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operation = transaction->operation;
	ctx->classifier_solver.classifier_metas[operation] =
		transaction->classifier_meta;
	ctx->classifier_solver.motive_solutions[operation] =
		transaction->motive_solution;
	ctx->classifier_solver.solution_revision = transaction->solution_revision;
	ctx->classifier_seed_suppressed[operation] = 0;
	transaction->active = 0;
	return 0;
}

static int operation_match_motive_rebuild_transaction_commit(
	struct compile_context* ctx,
	struct operation_match_motive_rebuild_transaction* transaction
) {
	if (!ctx || !transaction || !transaction->active ||
		transaction->operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operation = transaction->operation;
	const struct operation_classifier_meta_solution* candidate =
		&ctx->classifier_solver.classifier_metas[operation];
	if (candidate->classifier == PROTOTYPE_INVALID_ID) {
		return operation_match_motive_rebuild_transaction_rollback(
			ctx, transaction
		);
	}
	if (transaction->classifier_meta.classifier != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			transaction->classifier_meta.classifier,
			candidate->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* Validation may intern another representative. Keep the original
		 * solution and its evidence when the rebuilt equation is convertible. */
		return operation_match_motive_rebuild_transaction_rollback(
			ctx, transaction
		);
	}
	ctx->classifier_seed_suppressed[operation] = 0;
	transaction->active = 0;
	return 0;
}

/* Indexed branch pullback changes the Context in which branch classifiers are
 * represented. A motive materialized before that pullback is derived state,
 * so reconstruct it from the final branch equations instead of retaining a
 * source-Context snapshot or treating a branch-local classifier as expected
 * input for the whole Match. */
static int operation_solver_rebuild_refined_match_motives(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t pending_id = 0;
		pending_id < ctx->pending_match_typing_count;
		++pending_id) {
		uint32_t operation_id =
			ctx->pending_match_typings[pending_id].operation;
		if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			operation->first_case + operation->case_count >
				ctx->metadata->typed_occurrences.case_count) {
			continue;
		}
		int has_refined_branch = 0;
		int all_branches_closed = 1;
		for (uint32_t case_index = 0;
			case_index < operation->case_count;
			++case_index) {
			uint32_t operation_case_id = operation->first_case + case_index;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[operation_case_id];
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				has_refined_branch = 1;
			} else if (operation_case->refinement_status !=
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				all_branches_closed = 0;
			}
		}
		if (!has_refined_branch || !all_branches_closed) {
			continue;
		}

		struct operation_match_motive_rebuild_transaction transaction;
		if (operation_match_motive_rebuild_transaction_begin(
				ctx, operation_id, &transaction
			) != 0) {
			return -1;
		}

		int changed = 0;
		int rebuild_status = operation_solver_materialize_match_solution(
			ctx, operation_id, &changed
		);
		if (rebuild_status < 0) {
			operation_match_motive_rebuild_transaction_rollback(ctx, &transaction);
			return -1;
		}
		if (rebuild_status > 0 ||
			ctx->classifier_solver.classifier_metas[operation_id].classifier ==
				PROTOTYPE_INVALID_ID) {
			/* Some closed ordinary Matches still depend on classifier equations
			 * scheduled later in this pass. Rebuilding is a cache repair, so an
			 * unavailable replacement must leave the existing solution intact. */
			if (operation_match_motive_rebuild_transaction_rollback(
					ctx, &transaction
				) != 0) {
				return -1;
			}
			continue;
		}
		if (operation_match_motive_rebuild_transaction_commit(
				ctx, &transaction
			) != 0) {
			return -1;
		}
	}
	return 0;
}

/* Keep IH propagation entirely in the solver until its parent motive has
 * been materialized from a solved equation. */
static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	/* The Context action transports a source classifier into the refined branch,
	 * but that projection is only a seed. Once the owner motive is available the
	 * IH must still be reconstructed from it; otherwise a consumer expectation
	 * can survive as a parallel classifier authority until accepted replay. */
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		uint32_t projected_classifier;
		if (ctx->classifier_solver.classifier_metas[operation_id].classifier ==
				PROTOTYPE_INVALID_ID &&
			operation->source_classifier != PROTOTYPE_INVALID_ID &&
			prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				operation->source_classifier,
				operation->context_action_substitution,
				&projected_classifier
			) == 0) {
			if (operation_classifier_meta_solve(
				ctx, operation_id, projected_classifier
			) != 0) {
				return -1;
			}
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
				return -1;
			}
		}
	}
	uint32_t argument_occurrence;
	uint32_t parent_match = operation->ih_owner_occurrence;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
			&argument_occurrence
		) != 0 || parent_match >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[parent_match].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		fprintf(stderr,
			"IH materialization: invalid edge operation=%u parent=%u argument=%u\n",
			operation_id, parent_match, argument_occurrence);
		return -1;
	}
	if (operation_solver_solve_match(ctx, parent_match, p_changed) != 0) {
		fprintf(stderr, "IH materialization: parent match solve failed operation=%u match=%u\n",
			operation_id, parent_match);
		return -1;
	}
	uint32_t motive = ctx->classifier_solver.motive_solutions[parent_match].motive;
	if (motive == PROTOTYPE_INVALID_ID) {
		uint32_t parent_classifier = operation_solver_classifier(ctx, parent_match);
		uint32_t match_scrutinee;
		if (parent_classifier != PROTOTYPE_INVALID_ID &&
			parent_classifier < ctx->terms->term_count &&
			ctx->terms->terms[parent_classifier].tag == PROTOTYPE_TERM_APP &&
			compile_occurrence_child(
				ctx, parent_match, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&match_scrutinee
			) == 0 &&
			ctx->terms->terms[parent_classifier].as.app.argument ==
				ctx->metadata->typed_occurrences.occurrences[match_scrutinee].core_term) {
			motive = ctx->terms->terms[parent_classifier].as.app.function;
			while (motive < ctx->terms->term_count &&
				ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
				motive = ctx->terms->terms[motive].as.app.function;
			}
			if (motive >= ctx->terms->term_count ||
				ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
				fprintf(stderr,
					"IH materialization: recovered motive is not lambda operation=%u "
					"parent=%u motive=%u\n",
					operation_id, parent_match, motive);
				return -1;
			}
			ctx->classifier_solver.motive_solutions[parent_match].motive = motive;
			ctx->classifier_solver.motive_solutions[parent_match].state =
				OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		}
	}
	if (motive == PROTOTYPE_INVALID_ID) {
		if (operation_solver_validate_guarded_motive_occurrence(
				ctx, operation_id, parent_match, argument_occurrence
			) != 0) {
			fprintf(stderr,
				"IH materialization: guarded occurrence invalid operation=%u parent=%u\n",
				operation_id, parent_match);
			return -1;
		}
		if (operation_solver_set_ih_motive_application(
				ctx, operation_id, parent_match, argument_occurrence, p_changed
			) != 0) {
			fprintf(stderr,
				"IH materialization: motive application invalid operation=%u parent=%u "
				"argument=%u previous=%u count=%u\n",
				operation_id, parent_match, argument_occurrence,
				ctx->classifier_solver.ih_motive_application_ids[operation_id],
				ctx->classifier_solver.motive_application_count);
			return -1;
		}
		/* A constant motive equation M(_) == T is already a concrete solver
		 * solution for this IH occurrence.  Bind it now so CBPV wrappers can
		 * propagate through RETURN/THUNK/COMPUTATION_FOLD; the lambda for M is still
		 * materialized only after every Match equation has been checked. */
		uint32_t candidate =
			ctx->classifier_solver.motive_solutions[parent_match].constant_candidate;
		if (candidate != PROTOTYPE_INVALID_ID) {
			uint32_t previous = ctx->classifier_solver.classifier_metas[
				operation_id
			].classifier;
			return previous == PROTOTYPE_INVALID_ID ?
				operation_solver_bind(ctx, operation_id, candidate, p_changed) : 0;
		}
		return 0;
	}
	if (ctx->classifier_solver.ih_projected_motives[operation_id] == motive &&
		ctx->classifier_solver.classifier_metas[operation_id].classifier !=
			PROTOTYPE_INVALID_ID) {
		/* This exact owner motive has already been projected to the IH. */
		return 0;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[parent_match];
	uint32_t match_scrutinee;
	uint32_t scrutinee_classifier = compile_occurrence_child(
		ctx, parent_match, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
		&match_scrutinee
	) == 0 ? operation_solver_classifier(ctx, match_scrutinee) :
		PROTOTYPE_INVALID_ID;
	uint32_t field_classifier = PROTOTYPE_INVALID_ID;
	uint32_t field_context = operation->context_id;
	uint32_t field_action = PROTOTYPE_INVALID_ID;
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		const struct prototype_substitution* action =
			prototype_substitution_get(
				&ctx->metadata->substitutions,
				operation->context_action_substitution
			);
		if (!action || action->source_context != operation->context_id) {
			return -1;
		}
		/* Constructor field schemas are stated in the source branch telescope.
		 * A solved index equation may remove one of those binders from the pulled-
		 * back occurrence Context, so construct the schema before the action and
		 * then transport the resulting classifier through that same action. */
		field_context = action->target_context;
		field_action = operation->context_action_substitution;
	}
	if (match_operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[match_operation->core_term].tag == PROTOTYPE_TERM_MATCH &&
		operation->ih_case_index <
			ctx->terms->terms[match_operation->core_term].as.match.case_count) {
		uint32_t case_id = ctx->terms->terms[
			match_operation->core_term
		].as.match.first_case + operation->ih_case_index;
		if (case_id < ctx->terms->case_count) {
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[case_id];
			if (operation->ih_field_index < match_case->binder_count &&
				prototype_judgement_constructor_field_classifier(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					field_context,
					match_case->constructor_owner,
					match_case->constructor_id,
					&ctx->terms->case_binders[match_case->first_binder],
					operation->ih_field_index,
					operation->ih_field_index,
					&field_classifier
				) != 0) {
				fprintf(stderr,
					"IH materialization: field classifier failed operation=%u parent=%u "
					"case=%u field=%u context=%u\n",
					operation_id, parent_match, operation->ih_case_index,
					operation->ih_field_index, operation->context_id);
				return -1;
			}
		}
	}
	if (field_classifier != PROTOTYPE_INVALID_ID &&
		field_action != PROTOTYPE_INVALID_ID && prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			field_classifier,
			field_action,
			&field_classifier
		) != 0) {
		return -1;
	}
	uint32_t classifier;
	int lift_status = field_classifier == PROTOTYPE_INVALID_ID ||
		scrutinee_classifier == PROTOTYPE_INVALID_ID ? -2 :
		prototype_judgement_lift_recursive_field_classifier(
			ctx->terms,
			ctx->type_declarations,
			field_classifier,
			ctx->metadata->typed_occurrences.occurrences[argument_occurrence].core_term,
			scrutinee_classifier,
			motive,
			&classifier
		);
	if (lift_status != 0) {
		fprintf(stderr,
			"IH materialization: recursive field lift failed operation=%u status=%d "
			"field=%u scrutinee=%u motive=%u\n",
			operation_id, lift_status, field_classifier,
			scrutinee_classifier, motive);
		return -1;
	}
	int motive_effort = compile_effort_charge(
		ctx, PROTOTYPE_EFFORT_PHASE_MOTIVE, 1
	);
	if (motive_effort != 0) return motive_effort;
	for (uint32_t reduction = 0; reduction < 4; ++reduction) {
		struct prototype_term_normalization_result normalized_classifier;
		uint64_t normalization_limit = compile_effort_remaining(ctx);
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				classifier,
				normalization_limit,
				&normalized_classifier
			) != 0 || normalized_classifier.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			fprintf(stderr,
				"IH materialization: classifier normalization failed operation=%u classifier=%u\n",
				operation_id, classifier);
			return -1;
		}
		if (normalized_classifier.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			normalized_classifier.term_id == classifier) {
			break;
		}
		classifier = normalized_classifier.term_id;
	}
	if (operation_solver_close_unowned_effect_rows(
			ctx, operation_id, classifier, &classifier
		) != 0) {
		fprintf(stderr,
			"IH materialization: effect closure failed operation=%u classifier=%u\n",
			operation_id, classifier);
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.classifier_metas[operation_id].classifier;
	if (previous < ctx->terms->term_count &&
		ctx->terms->terms[previous].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			ctx->terms->terms[previous].as.computation_type.result,
			classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The recursive occurrence already crossed a CBPV boundary. Its motive
		 * computes the result classifier, while the occurrence itself retains the
		 * enclosing computation and effect row. */
		ctx->classifier_solver.ih_projected_motives[operation_id] = motive;
		return 0;
	}
	if (previous != PROTOTYPE_INVALID_ID) {
		struct prototype_term_classifier_view previous_view;
		struct prototype_term_classifier_view lifted_view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, previous, &previous_view
			) == 0 && prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, classifier, &lifted_view
			) == 0 && previous_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION && lifted_view.category ==
				PROTOTYPE_TERM_CATEGORY_VALUE && prototype_term_computation_type(
				ctx->terms, previous_view.effect_row, classifier,
				previous_view.totality, &classifier
			) != 0) {
			return -1;
		}
	}
	if (previous == PROTOTYPE_INVALID_ID) {
		uint32_t prior_projected_motive =
			ctx->classifier_solver.ih_projected_motives[operation_id];
		int bind_status = operation_solver_bind(
			ctx, operation_id, classifier, p_changed
		);
		if (bind_status == 0) {
			ctx->classifier_solver.ih_projected_motives[operation_id] = motive;
			if (prior_projected_motive != PROTOTYPE_INVALID_ID) {
				return operation_solver_invalidate_dependent_classifiers(
					ctx, operation_id
				);
			}
		}
		return bind_status;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
		((ctx->classifier_solver.recursive_equation_owners[operation_id] ==
				parent_match && ctx->classifier_solver.motive_solutions[
					parent_match
				].state == OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY) ||
		 ctx->classifier_solver.motive_solutions[parent_match].state ==
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED)) {
		uint32_t previous_whnf;
		uint32_t classifier_whnf;
		if (prototype_judgement_classifier_value_whnf(
				ctx->terms, ctx->type_declarations, previous, &previous_whnf
			) != 0 || prototype_judgement_classifier_value_whnf(
				ctx->terms, ctx->type_declarations, classifier, &classifier_whnf
			) != 0) {
			return -1;
		}
		if (ctx->terms->terms[previous_whnf].tag == PROTOTYPE_TERM_PI &&
			ctx->terms->terms[classifier_whnf].tag != PROTOTYPE_TERM_PI) {
			/* A neutral value motive cannot yet recover the callable telescope of
			 * an IH over an indexed recursive field. Preserve the constructor-schema
			 * projection so its applications can contribute the missing indexed
			 * equation. Replacing it here would make that equation unreachable. */
			return 0;
		}
		/* The recursive equation marks the earlier classifier as a provisional
		 * projection. Its polarity is irrelevant: the materialized owner motive is
		 * authoritative for value, Pi, and computation results alike. */
		if (operation_classifier_meta_invalidate(ctx, operation_id) != 0 ||
			operation_classifier_meta_solve(
				ctx, operation_id, classifier
			) != 0) {
			return -1;
		}
		ctx->classifier_solver.ih_projected_motives[operation_id] = motive;
		*p_changed = 1;
		return operation_solver_invalidate_dependent_classifiers(
			ctx, operation_id
		);
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* A unary motive inferred before an IH consumer contributes its indexed
		 * equation is provisional. The constructor telescope projection remains
		 * the only typed classifier available at this fixed-point stage. Keep it
		 * until expected refinement records the recursive equation; accepted
		 * materialization will then either replace it from that equation or reject
		 * the jointly validated motive. */
		return 0;
	}
	int widen_status = operation_solver_widen_computation_binding(
		ctx, operation_id, classifier, p_changed
	);
	if (widen_status != 0) {
		fprintf(stderr,
			"IH materialization: computation widening failed operation=%u previous=%u "
			"classifier=%u status=%d\n",
			operation_id, previous, classifier, widen_status);
	} else {
		ctx->classifier_solver.ih_projected_motives[operation_id] = motive;
	}
	return widen_status;
}
