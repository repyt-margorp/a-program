static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t match_typed_projection,
	int* p_changed
);

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
);

static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t typed_projection,
	int* p_changed
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);

static int operation_solver_classifier_is_strictly_more_general(
	const struct prototype_term_db* terms,
	uint32_t candidate,
	uint32_t current,
	int* p_more_general
);

static int operation_solver_binding_classifier_is_inferred(
	struct compile_context* ctx,
	uint32_t binding_id,
	uint32_t binding_context
) {
	const struct prototype_context* binding = ctx ? prototype_context_get(
		&ctx->metadata->contexts, binding_context
	) : NULL;
	if (!binding || binding->binding_id != binding_id) {
		return 0;
	}
	const struct prototype_context_classifier_equation* equation =
		operation_binder_classifier_meta_get(
			ctx, prototype_context_classifier_equation(binding)
		);
	if (equation && equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		equation = operation_binder_classifier_meta_get(ctx, equation->key_value);
	}
	if (equation && equation->source_classifier_authority ==
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_INFERRED) {
		return 1;
	}
	if (equation && equation->source_classifier_authority ==
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED) {
		return 0;
	}
	/* Compatibility for Contexts built outside frontend lowering. New compiler
	 * equations always carry explicit authority. */
	if (equation && equation->source_ast_binder_id != PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (binding->extension_kind == PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ||
		(equation && equation->source_ast_binder_id == PROTOTYPE_INVALID_ID)) {
		return 1;
	}
	const struct prototype_typing_binder_owner* owner =
		operation_binder_owner_find(ctx, binding_id, binding_context);
	return owner && owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID;
}

static int operation_solver_projected_binding_classifier(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->typing || !constraint || !p_classifier ||
		constraint->kind != OPERATION_CONSTRAINT_BINDER_TYPE ||
		constraint->owner_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	uint32_t context_id;
	uint32_t substitution_id;
	int projection_status = operation_typed_projection_materialized_context(
		ctx,
		constraint->owner_typed_projection,
		&context_id,
		&substitution_id
	);
	if (projection_status != 0) {
		return projection_status;
	}
	(void)substitution_id;
	uint32_t binding_context;
	int binding_status = prototype_context_find_binding(
		&ctx->metadata->contexts,
		context_id,
		constraint->payload.classifier.goal.binder_type.binding_id,
		&binding_context
	);
	if (binding_status != 0) {
		return binding_status;
	}
	const struct prototype_context* binding = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	if (!binding) {
		return -1;
	}
	if (binding->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
		binding->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) {
		return -1;
	}
	if (binding->extension_kind == PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT &&
		constraint->operand_count != 1) {
		return -1;
	}
	if (binding->extension_kind == PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) {
		uint32_t producer_projection =
			operation_solver_constraint_operand_projection(ctx, constraint, 0);
		uint32_t producer_classifier = operation_solver_classifier_for_projection(
			ctx, producer_projection
		);
		if (producer_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t binder_classifier = producer_classifier;
		if (ctx->terms->terms[producer_classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			struct prototype_classifier_view producer_view;
			if (prototype_judgement_classifier_view(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					producer_classifier,
					&producer_view
				) != 0) {
				return -1;
			}
			if (producer_view.category !=
					PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
				producer_view.computation_kind !=
					PROTOTYPE_COMPUTATION_KIND_RETURNING) {
				return 1;
			}
			binder_classifier = producer_view.result;
		}
		/* The sequence-producer operand is the relation equation for this exact
		 * binder use. Context.classifier_equation is only its lexical schema and
		 * cannot represent a result specialized by branch-local values. */
		return operation_solver_term_in_projection_context(
			ctx,
			constraint->owner_typed_projection,
			binder_classifier,
			p_classifier
		);
	}
	/* The producer edge is a scheduling dependency. The classifier equation of
	 * the materialized Context binding is the semantic authority for ordinary
	 * value binders and already contains any reindexing required by that Context. */
	*p_classifier = operation_context_classifier_term(ctx, binding_context);
	if (*p_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	const struct prototype_context_classifier_equation* equation =
		operation_binder_classifier_meta_get(
			ctx, prototype_context_classifier_equation(binding)
		);
	const struct prototype_context_classifier_equation* source_equation = equation;
	if (equation && equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		source_equation = operation_binder_classifier_meta_get(
			ctx, equation->key_value
		);
	}
	uint32_t projected = operation_solver_classifier_for_projection(
		ctx, constraint->owner_typed_projection
	);
	if (source_equation && operation_solver_binding_classifier_is_inferred(
			ctx,
			constraint->payload.classifier.goal.binder_type.binding_id,
			binding_context
		) &&
		projected != PROTOTYPE_INVALID_ID && projected != *p_classifier) {
		int source_is_more_general;
		if (operation_solver_classifier_is_strictly_more_general(
				ctx->terms, *p_classifier, projected, &source_is_more_general
			) != 0) {
			return -1;
		}
		if (source_is_more_general) {
			/* A sequence-result Context equation is the binder schema. Once another
			 * constraint has specialized the exact TypedProjection, the schema
			 * premise validates that answer but must not overwrite it. */
			*p_classifier = projected;
		}
	}
	return 0;
}

static int operation_solver_lambda_projection_binding_context(
	struct compile_context* ctx,
	uint32_t lambda_typed_projection,
	uint32_t* p_body_typed_projection,
	uint32_t* p_binding_context
) {
	if (!ctx || !p_body_typed_projection || !p_binding_context ||
		lambda_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typed_projection* lambda_projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, lambda_typed_projection
		);
	if (!lambda_projection || lambda_projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* lambda =
		&ctx->metadata->typed_occurrences.occurrences[
			lambda_projection->source_occurrence
		];
	if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		lambda->binding_id == PROTOTYPE_INVALID_ID ||
		operation_solver_projected_child(
			ctx, lambda_typed_projection, PROTOTYPE_TERM_CHILD_BODY, 0,
			p_body_typed_projection
		) != 0) {
		return -1;
	}
	uint32_t body_context;
	uint32_t body_substitution;
	int context_status = operation_typed_projection_materialized_context(
		ctx, *p_body_typed_projection, &body_context, &body_substitution
	);
	if (context_status != 0) {
		return context_status;
	}
	(void)body_substitution;
	return prototype_context_find_binding(
		&ctx->metadata->contexts,
		body_context,
		lambda->binding_id,
		p_binding_context
	);
}

static int operation_solver_lambda_projection_binder_classifier(
	struct compile_context* ctx,
	uint32_t lambda_typed_projection,
	uint32_t* p_classifier
) {
	uint32_t body_typed_projection;
	uint32_t binding_context;
	if (!p_classifier) {
		return -1;
	}
	int status = operation_solver_lambda_projection_binding_context(
		ctx, lambda_typed_projection, &body_typed_projection, &binding_context
	);
	if (status != 0) {
		return status;
	}
	uint32_t lambda_equation;
	if (operation_solver_projected_classifier_equation_id(
			ctx, lambda_typed_projection, &lambda_equation
		) != 0 || !prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, lambda_equation,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return -1;
	}
	int relation_status =
		operation_solver_projected_sequence_binder_classifier(
			ctx, lambda_typed_projection, p_classifier
		);
	if (relation_status < 0) {
		return -1;
	}
	if (relation_status == 0) {
		return 0;
	}
	if (relation_status == 2) {
		return 1;
	}
	const struct prototype_typing_constraint_solution* lambda_solution =
		&ctx->typing->constraints.solutions[lambda_equation];
	if (lambda_solution->projected_binder_classifier != PROTOTYPE_INVALID_ID) {
		*p_classifier = lambda_solution->projected_binder_classifier;
		return 0;
	}
	(void)body_typed_projection;
	*p_classifier = operation_context_classifier_term(ctx, binding_context);
	return *p_classifier == PROTOTYPE_INVALID_ID ? 1 : 0;
}

static int operation_solver_seed_indexed_motive_from_ih_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	int* p_changed
);

static const struct prototype_typing_constraint*
operation_motive_ih_equation_for_projection_const(
	const struct compile_context* ctx,
	uint32_t typed_projection
);

static int operation_motive_ih_projection_is_current(
	const struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t motive
);

static int operation_motive_ih_equation_is_active(
	const struct compile_context* ctx,
	uint32_t constraint_id,
	uint32_t owner_match
);

struct operation_solver_context_scope_query {
	const struct prototype_context_db* contexts;
	uint32_t context_id;
};

static int operation_solver_binding_outside_context(
	void* context,
	uint32_t binding_id
) {
	const struct operation_solver_context_scope_query* query = context;
	uint32_t ignored_context;
	if (!query) {
		return -1;
	}
	int status = prototype_context_find_binding(
		query->contexts, query->context_id, binding_id, &ignored_context
	);
	return status < 0 ? -1 : status > 0;
}

static int operation_solver_publish_context_classifier(
	struct compile_context* ctx,
	uint32_t equation_id,
	uint32_t classifier,
	int replace,
	int* p_changed
) {
	if (!ctx || !p_changed || classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_context_classifier_equation* equation =
		operation_binder_classifier_meta_get(ctx, equation_id);
	if (!equation) {
		return -1;
	}
	int equation_changed = 0;
	if (equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		struct prototype_context_classifier_equation* source_equation =
			operation_binder_classifier_meta_get(ctx, equation->key_value);
		if (!source_equation || equation->key_auxiliary >=
				ctx->metadata->substitutions.substitution_count) {
			return -1;
		}
		struct operation_solver_context_scope_query scope = {
			.contexts = &ctx->metadata->contexts,
			.context_id = source_equation->parent_context
		};
		int outside_context;
		uint32_t projected_classifier;
		if (prototype_term_any_free_binding(
				ctx->terms,
				classifier,
				operation_solver_binding_outside_context,
				&scope,
				&outside_context
			) != 0) {
			return -1;
		}
		if (outside_context) {
			return 1;
		}
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				classifier,
				equation->key_auxiliary,
				&projected_classifier
			) != 0) {
			return -1;
		}
		if (!prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				projected_classifier,
				classifier
			)) {
			return 1;
		}
		if (source_equation->answer == PROTOTYPE_INVALID_ID) {
			if (prototype_context_classifier_equation_publish(
					&ctx->metadata->contexts,
					source_equation->id,
					classifier
				) != 0) {
				return -1;
			}
			*p_changed = 1;
			equation_changed = 1;
		} else if (!prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				source_equation->answer,
				classifier
			)) {
			if (!replace || prototype_context_classifier_equation_transition(
					&ctx->metadata->contexts,
					source_equation->id,
					source_equation->answer,
					classifier
				) != 0) {
				return -1;
			}
			*p_changed = 1;
			equation_changed = 1;
		}
		int reindex_changed = 0;
		if (operation_solver_solve_context_classifier_reindexes(
				ctx, &reindex_changed
			) != 0) {
			return -1;
		}
		if (reindex_changed) {
			*p_changed = 1;
			equation_changed = 1;
		}
		equation = operation_binder_classifier_meta_get(ctx, equation_id);
		if (!equation || equation->answer == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		/* Reindex equations preserve the source classifier schema. Distinct
		 * Universe metavariables are deliberately not DefEq; their ordering is
		 * discharged by UniverseDB after evidence reification. Requiring exact
		 * conversion here would reject a compatible source/projected pair before
		 * that constraint can be recorded. */
		if (!prototype_judgement_classifier_compatible(
				ctx->terms, ctx->type_declarations,
				equation->answer, classifier
			)) {
			return -1;
		}
	} else if (equation->answer == PROTOTYPE_INVALID_ID) {
		if (prototype_context_classifier_equation_publish(
				&ctx->metadata->contexts, equation_id, classifier
			) != 0) {
			return -1;
		}
		*p_changed = 1;
		equation_changed = 1;
	} else if (!prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			equation->answer,
			classifier
		)) {
		if (!replace || prototype_context_classifier_equation_transition(
				&ctx->metadata->contexts,
				equation_id,
				equation->answer,
				classifier
			) != 0) {
			return -1;
		}
		*p_changed = 1;
		equation_changed = 1;
	}

	if (equation_changed) {
		if (operation_solver_enqueue_context_classifier_equation_evidence(
				ctx, equation->id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_bind_lambda_projection_binder_classifier(
	struct compile_context* ctx,
	uint32_t lambda_typed_projection,
	uint32_t classifier,
	int replace,
	int* p_changed
) {
	uint32_t body_typed_projection;
	uint32_t binding_context;
	if (!ctx || !p_changed || classifier >= ctx->terms->term_count) {
		return -1;
	}
	int status = operation_solver_lambda_projection_binding_context(
		ctx, lambda_typed_projection, &body_typed_projection, &binding_context
	);
	if (status != 0) {
		return status;
	}
	const struct prototype_context* binding = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	if (!binding) {
		return -1;
	}
	uint32_t equation_id = prototype_context_classifier_equation(binding);
	struct prototype_context_classifier_equation* equation =
		operation_binder_classifier_meta_get(ctx, equation_id);
	if (!equation || equation->extension_context != binding_context) {
		return -1;
	}
	struct prototype_context_classifier_equation* source_equation = equation;
	if (equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		source_equation = operation_binder_classifier_meta_get(
			ctx, equation->key_value
		);
	}
	if (!source_equation) {
		return -1;
	}
	int inferred_binder = operation_solver_binding_classifier_is_inferred(
		ctx, binding->binding_id, binding_context
	);
	uint32_t lambda_equation;
	if (operation_solver_projected_classifier_equation_id(
			ctx, lambda_typed_projection, &lambda_equation
		) != 0 || !prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, lambda_equation,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return -1;
	}
	struct prototype_typing_constraint_solution* lambda_solution =
		&ctx->typing->constraints.solutions[lambda_equation];
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
		const struct prototype_typing_binder_owner* trace_owner =
			operation_binder_owner_find(ctx, binding->binding_id, binding_context);
		fprintf(
			stderr,
			"lambda-binder-bind projection=%u binding=%u context=%u equation=%u "
			"source=%u ast=%u extension=%u owner-computation=%u inferred=%d "
			"current=%u candidate=%u replace=%d\n",
			lambda_typed_projection,
			binding->binding_id,
			binding_context,
			equation_id,
			source_equation->id,
			source_equation->source_ast_binder_id,
			binding->extension_kind,
			trace_owner ? trace_owner->bound_computation_occurrence :
				PROTOTYPE_INVALID_ID,
			inferred_binder,
			source_equation->answer,
			classifier,
			replace
		);
	}
	int publish_source = !inferred_binder ||
		source_equation->answer == PROTOTYPE_INVALID_ID;
	int replace_source = replace;
	if (!inferred_binder && source_equation->answer != PROTOTYPE_INVALID_ID &&
		!prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			source_equation->answer,
			classifier
		)) {
		/* A fixed source equation is an immutable schema. Its open effect rows may
		 * still be instantiated for this TypedProjection without rewriting that
		 * schema. */
		uint32_t instantiated_source;
		int instantiate_status =
			prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				source_equation->answer,
				classifier,
				&instantiated_source
			);
		if (instantiate_status < 0) {
			return -1;
		}
		if (instantiate_status == 0 &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				instantiated_source,
				classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			publish_source = 0;
		} else {
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(stderr, "lambda-binder-fixed-mismatch current: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, source_equation->answer
				);
				fprintf(stderr, "\nlambda-binder-fixed-mismatch candidate: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, classifier
				);
				fputc('\n', stderr);
			}
			return 1;
		}
	}
	if (inferred_binder && source_equation->answer != PROTOTYPE_INVALID_ID) {
		struct prototype_term_conversion_result source_conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				source_equation->answer,
				classifier
			);
		if (source_conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			classifier = source_equation->answer;
			publish_source = 0;
		} else if (replace) {
			/* A stronger direct input may replace an inferred source answer only when
			 * it is expressible in the source Context. A reindexed Context equation
			 * can expose either a genuine source refinement or a projection-local
			 * classifier; free-binding scope, not the reindex tag alone, distinguishes
			 * those cases. */
			publish_source = 1;
			if (equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
				struct operation_solver_context_scope_query source_scope = {
					.contexts = &ctx->metadata->contexts,
					.context_id = source_equation->parent_context
				};
				int outside_source_context;
				if (prototype_term_any_free_binding(
						ctx->terms,
						classifier,
						operation_solver_binding_outside_context,
						&source_scope,
						&outside_source_context
					) != 0) {
					return -1;
				}
				publish_source = !outside_source_context;
			}
			replace_source = publish_source;
		} else {
			int candidate_more_general;
			int candidate_effect_superset =
				prototype_judgement_classifier_effect_row_includes(
					ctx->terms,
					ctx->type_declarations,
					classifier,
					source_equation->answer
				);
			int source_effect_superset =
				prototype_judgement_classifier_effect_row_includes(
					ctx->terms,
					ctx->type_declarations,
					source_equation->answer,
					classifier
				);
			if (candidate_effect_superset < 0 || source_effect_superset < 0 ||
				operation_solver_classifier_is_strictly_more_general(
					ctx->terms,
					classifier,
					source_equation->answer,
					&candidate_more_general
				) != 0) {
				return -1;
			}
			publish_source = candidate_more_general ||
				(candidate_effect_superset && !source_effect_superset);
			if (publish_source) {
				replace_source = 1;
			} else if (source_effect_superset) {
				classifier = source_equation->answer;
			} else {
				/* Neither equation refines the other. The sequencing constraint must
				 * push the established binder domain back to its producer instead of
				 * installing a second projection authority. */
				return 1;
			}
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"lambda-binder-join projection=%u equation=%u source=%u current=%u "
					"selected=%u publish=%d candidate-effects=%d source-effects=%d\n",
					lambda_typed_projection, equation_id, source_equation->id,
					source_equation->answer, classifier, publish_source,
					candidate_effect_superset, source_effect_superset
				);
				fprintf(stderr, "  source-current: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, source_equation->answer
				);
				fprintf(stderr, "\n  source-candidate: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, classifier
				);
				fputc('\n', stderr);
			}
		}
	}
	int binding_changed = 0;
	if (publish_source) {
		int publish_status = operation_solver_publish_context_classifier(
			ctx,
			inferred_binder ? source_equation->id : equation_id,
			classifier,
			replace_source,
			&binding_changed
		);
		if (publish_status != 0) {
			return publish_status;
		}
	}
	int projection_changed = inferred_binder &&
		lambda_solution->projected_binder_classifier != classifier;
	if (projection_changed) {
		lambda_solution->projected_binder_classifier = classifier;
	}
	if (!binding_changed && !projection_changed) {
		return 0;
	}
	*p_changed = 1;
	if (operation_solver_projection_answer_transition(
			ctx,
			lambda_typed_projection,
			OPERATION_SOLVER_ANSWER_INPUT_CHANGED,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			p_changed
		) != 0 || prototype_typing_constraint_transition(
			&ctx->typing->constraints,
			lambda_equation,
			OPERATION_CONSTRAINT_STATE_PENDING,
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
		) != 0 || operation_solver_enqueue_constraint(
			ctx, lambda_equation, PROTOTYPE_COMPILE_ENQUEUE_INVALIDATION
		) != 0) {
		return -1;
	}
	return 0;
}

struct operation_solver_free_binding_difference_query {
	const struct prototype_term_db* terms;
	uint32_t other;
};

static int operation_solver_free_binding_absent_from_other(
	void* context,
	uint32_t binding_id
) {
	const struct operation_solver_free_binding_difference_query* query = context;
	return query && !prototype_term_contains_free_binding(
		query->terms, query->other, binding_id
	);
}

static int operation_solver_classifier_is_strictly_more_general(
	const struct prototype_term_db* terms,
	uint32_t candidate,
	uint32_t current,
	int* p_more_general
) {
	if (!terms || !p_more_general || candidate >= terms->term_count ||
		current >= terms->term_count) {
		return -1;
	}
	struct operation_solver_free_binding_difference_query candidate_query = {
		.terms = terms,
		.other = current
	};
	struct operation_solver_free_binding_difference_query current_query = {
		.terms = terms,
		.other = candidate
	};
	int candidate_has_extra;
	int current_has_extra;
	if (prototype_term_any_free_binding(
			terms,
			candidate,
			operation_solver_free_binding_absent_from_other,
			&candidate_query,
			&candidate_has_extra
		) != 0 || prototype_term_any_free_binding(
			terms,
			current,
			operation_solver_free_binding_absent_from_other,
			&current_query,
			&current_has_extra
		) != 0) {
		return -1;
	}
	*p_more_general = candidate_has_extra && !current_has_extra;
	return 0;
}

static int operation_solver_publish_projected_binding_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t classifier,
	int* p_changed
) {
	uint32_t aggregate_id;
	if (!ctx || !p_changed || operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &aggregate_id
		) != 0 || !prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, aggregate_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return -1;
	}
	uint32_t binder_equation = PROTOTYPE_INVALID_ID;
	for (uint32_t premise = ctx->typing->constraints.
			first_classifier_premise_for_projection[typed_projection];
		premise != PROTOTYPE_INVALID_ID;
		premise = ctx->typing->constraints.next_classifier_premise[premise]) {
		if (!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, premise,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			)) {
			return -1;
		}
		if (ctx->typing->constraints.constraints[premise].kind !=
				OPERATION_CONSTRAINT_BINDER_TYPE) {
			continue;
		}
		if (binder_equation != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		binder_equation = premise;
	}
	if (binder_equation == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	return operation_solver_equation_candidate_transition(
		ctx, typed_projection, binder_equation, classifier, p_changed
	);
}

static int operation_solver_bind_projected_variable_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || expected_classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* variable =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	if (variable->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		variable->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t materialized_context;
	uint32_t materialized_substitution;
	int context_status = operation_typed_projection_materialized_context(
		ctx,
		typed_projection,
		&materialized_context,
		&materialized_substitution
	);
	if (context_status != 0) {
		return context_status;
	}
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
		fprintf(
			stderr,
			"projected-variable-context projection=%u binding=%u context=%u "
			"substitution=%u\n",
			typed_projection, variable->binding_id, materialized_context,
			materialized_substitution
		);
	}
	uint32_t binding_context;
	int binding_status = prototype_context_find_binding(
		&ctx->metadata->contexts,
		materialized_context,
		variable->binding_id,
		&binding_context
	);
	int binding_from_candidate_context = 0;
	if (binding_status != 0) {
		const struct prototype_context_projection* context_projection =
			prototype_context_projection_get(
				&ctx->typing->context_projections,
				projection->context_projection
			);
		if (!context_projection || prototype_context_find_binding(
				&ctx->metadata->contexts,
				context_projection->candidate_context,
				variable->binding_id,
				&binding_context
			) != 0) {
			return binding_status;
		}
		binding_from_candidate_context = 1;
	}
	const struct prototype_context* binding = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	if (!binding || binding->binding_id != variable->binding_id) {
		return -1;
	}
	uint32_t equation_id = prototype_context_classifier_equation(binding);
	const struct prototype_context_classifier_equation* equation =
		operation_binder_classifier_meta_get(ctx, equation_id);
	const struct prototype_context_classifier_equation* source_equation = equation;
	if (equation && equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		source_equation = operation_binder_classifier_meta_get(
			ctx, equation->key_value
		);
	}
	int inferred_binder = source_equation &&
		operation_solver_binding_classifier_is_inferred(
			ctx, variable->binding_id, binding_context
		);
	uint32_t binding_classifier = operation_context_classifier_term(
		ctx, binding_context
	);
	if (binding_from_candidate_context &&
		binding_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t contextual_classifier;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				binding_classifier,
				materialized_substitution,
				&contextual_classifier
			) != 0 || prototype_judgement_classifier_value_whnf(
				ctx->terms,
				ctx->type_declarations,
				contextual_classifier,
				&binding_classifier
			) != 0) {
			return -1;
		}
	}
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
		fprintf(
			stderr,
			"projected-variable-expected projection=%u occurrence=%u binding=%u "
			"materialized-context=%u substitution=%u binding-context=%u equation=%u "
			"classifier=%u expected=%u inferred=%d\n",
			typed_projection, projection->source_occurrence,
			variable->binding_id, materialized_context,
			materialized_substitution, binding_context, equation_id,
			binding_classifier, expected_classifier, inferred_binder
		);
	}
	if (!equation) {
		return -1;
	}
	int publish_status = 0;
	if (binding_classifier == PROTOTYPE_INVALID_ID) {
		publish_status = operation_solver_publish_context_classifier(
			ctx, equation_id, expected_classifier, 0, p_changed
		);
		binding_classifier = operation_context_classifier_term(
			ctx, binding_context
		);
	}
	if (publish_status != 0) {
		return publish_status;
	}
	if (binding_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	/* A TypedProjection receives the Context equation through its substitution.
	 * The source equation owns the binder family. The consuming projection owns
	 * its exact contextual answer; a reindex-specific answer must not be written
	 * back into the source equation. */
	uint32_t resolved_classifier = binding_classifier;
	if (!prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			binding_classifier,
			expected_classifier
		)) {
		if (inferred_binder && equation->key_kind ==
				CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
			resolved_classifier = expected_classifier;
			goto bind_projection;
		}
		/* Sequencing continuations are generated binders: their source
		 * classifier is synthesized from all contextual uses.  A later
		 * projection can expose a more general source-context expression than
		 * the first solved branch.  Replace that generated source answer, then
		 * derive every projected answer again through its substitution.
		 *
		 * Source binders with an AST identity are annotations.  Expected types
		 * only check those annotations and must never rewrite them. */
		int candidate_is_more_general = 0;
		if (source_equation && source_equation->answer != PROTOTYPE_INVALID_ID &&
			operation_solver_classifier_is_strictly_more_general(
				ctx->terms,
				expected_classifier,
				source_equation->answer,
				&candidate_is_more_general
			) != 0) {
			return -1;
		}
		if (inferred_binder && candidate_is_more_general) {
			struct operation_solver_context_scope_query source_scope = {
				.contexts = &ctx->metadata->contexts,
				.context_id = source_equation->parent_context
			};
			int outside_source_context;
			if (prototype_term_any_free_binding(
					ctx->terms,
					expected_classifier,
					operation_solver_binding_outside_context,
					&source_scope,
					&outside_source_context
				) != 0) {
				return -1;
			}
			if (!outside_source_context) {
				int refinement_changed = 0;
				publish_status = operation_solver_publish_context_classifier(
					ctx,
					source_equation->id,
					expected_classifier,
					1,
					&refinement_changed
				);
				if (publish_status == 0) {
					*p_changed |= refinement_changed;
				} else if (publish_status < 0) {
					return -1;
				}
			}
		}
		uint32_t instantiated_binding;
		int instantiate_status =
			prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				binding_classifier,
				expected_classifier,
				&instantiated_binding
			);
		if (instantiate_status < 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			fprintf(
				stderr,
				"projected-binder-effect-instance projection=%u source=%u expected=%u "
				"status=%d instantiated=%u conversion=%d\n",
				typed_projection,
				binding_classifier,
				expected_classifier,
				instantiate_status,
				instantiated_binding,
				instantiate_status == 0 ?
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						instantiated_binding,
						expected_classifier
					).status : PROTOTYPE_TERM_CONVERSION_INVALID
			);
		}
		if (instantiate_status == 0 &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				instantiated_binding,
				expected_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			resolved_classifier = instantiated_binding;
			goto bind_projection;
		}
		if (source_equation && source_equation->answer != PROTOTYPE_INVALID_ID) {
			uint32_t contextual_source;
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					source_equation->answer,
					materialized_substitution,
					&contextual_source
				) == 0 && prototype_judgement_classifier_value_whnf(
					ctx->terms,
					ctx->type_declarations,
					contextual_source,
					&contextual_source
				) == 0 && prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					contextual_source,
					expected_classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
					resolved_classifier = contextual_source;
					goto bind_projection;
				}
		}
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			const struct prototype_typing_binder_owner* trace_owner =
				operation_binder_owner_find(
					ctx, variable->binding_id, binding_context
				);
			fprintf(
				stderr,
				"projected-binder classifier mismatch projection=%u binding=%u "
				"current=%u expected=%u equation=%u key=%d context=%u extension=%d "
				"source=%u source-answer=%u source-ast=%u authority=%d inferred=%d owner-fold=%u "
				"owner-computation=%u\n",
				typed_projection,
				variable->binding_id,
				binding_classifier,
				expected_classifier,
				equation_id,
				equation->key_kind,
				binding_context,
				binding->extension_kind,
				source_equation ? source_equation->id : PROTOTYPE_INVALID_ID,
				source_equation ? source_equation->answer : PROTOTYPE_INVALID_ID,
				source_equation ? source_equation->source_ast_binder_id :
					PROTOTYPE_INVALID_ID,
				source_equation ? source_equation->source_classifier_authority :
					PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_UNSPECIFIED,
				inferred_binder,
				trace_owner ? trace_owner->sequence_fold_occurrence :
					PROTOTYPE_INVALID_ID,
				trace_owner ? trace_owner->bound_computation_occurrence :
					PROTOTYPE_INVALID_ID
			);
			fprintf(stderr, "  current: ");
			prototype_type_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, binding_classifier
			);
			fprintf(stderr, "\n  expected: ");
			prototype_type_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, expected_classifier
			);
			fputc('\n', stderr);
		}
		/* This helper only offers a refinement. A fixed binder mismatch is a
		 * relation result, not an internal Solver failure and not an unsolved
		 * classifier. The owning PI/expected equation compares the unchanged
		 * classifier and records the contradiction with its own evidence. */
		return 1;
	}

bind_projection:
	uint32_t projected_classifier = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
	if (projected_classifier == PROTOTYPE_INVALID_ID) {
		return operation_solver_publish_projected_binding_expected(
			ctx, typed_projection, resolved_classifier, p_changed
		);
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			projected_classifier,
			resolved_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	/* Compatibility is sufficient for checking a source binder annotation, but
	 * not for selecting the answer of one Context-indexed projection. The latter
	 * must retain the exact classifier obtained after its Context action. */
	return operation_solver_publish_projected_binding_expected(
		ctx, typed_projection, resolved_classifier, p_changed
	);
}

static int operation_solver_classifier_goal_is_satisfied(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* goal,
	int* p_reason
) {
	if (!ctx || !ctx->metadata || !goal || !p_reason ||
		goal->source_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
		!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, goal->id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return 0;
	}
	struct prototype_typing_constraint_solution* result =
		&ctx->typing->constraints.solutions[goal->id];
	uint32_t target = operation_solver_classifier_for_projection(
		ctx, goal->owner_typed_projection
	);
	/* A motive equation proves that one branch satisfies the Match motive.  The
	 * Match classifier is produced only after those equations are proved, so it
	 * is a consequence rather than a premise of this constraint. */
	if (target == PROTOTYPE_INVALID_ID &&
		goal->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION) {
		return 0;
	}
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_EQUAL: {
			if (goal->operand_count != 1) {
				return -1;
			}
			uint32_t referenced = operation_solver_classifier_for_projection(
				ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
			);
			if (referenced == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, target, referenced
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_REFERENCE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			if (result->conversion_goal.result.status !=
					PROTOTYPE_TERM_CONVERSION_EQUAL &&
				!result->classifier_compatibility_validated) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (goal->operand_count == 0) {
				return 0;
			}
			if (operation_solver_classifier_for_projection(
					ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			if (goal->payload.classifier.goal.pi.role ==
					OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
				uint32_t binder_classifier;
				int binder_status =
					operation_solver_lambda_projection_binder_classifier(
						ctx, goal->owner_typed_projection, &binder_classifier
					);
				if (binder_status < 0) {
					return -1;
				}
				if (binder_status > 0) {
					return 0;
				}
			} else {
				if (goal->operand_count != 2) {
					return -1;
				}
				if (operation_solver_classifier_for_projection(
						ctx, operation_solver_constraint_operand_projection(ctx, goal, 1)
					) == PROTOTYPE_INVALID_ID) {
					return 0;
				}
			}
			*p_reason = goal->payload.classifier.goal.pi.role ==
				OPERATION_CLASSIFIER_PI_GOAL_INTRO ?
				OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED :
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PI_DOMAIN_RELATION:
			if (result->result_term == PROTOTYPE_INVALID_ID) return 0;
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION:
			if (result->result_term == PROTOTYPE_INVALID_ID) return 0;
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_SEQUENCE_BINDER_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PROJECTED_CLASSIFIER:
			if (result->result_term == PROTOTYPE_INVALID_ID) return 0;
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			if (goal->operand_count != 2) {
				return -1;
			}
			if (operation_solver_classifier_for_projection(
					ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			if (operation_solver_classifier_for_projection(
					ctx, operation_solver_constraint_operand_projection(ctx, goal, 1)
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_BINDER_TYPE: {
			uint32_t binder_classifier;
			int binding_status = operation_solver_projected_binding_classifier(
				ctx, goal, &binder_classifier
			);
			if (binding_status < 0) {
				return -1;
			}
			if (binding_status > 0) {
				return 0;
			}
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, target, binder_classifier
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_BINDER_TYPE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
			uint32_t match_operation =
				goal->source_occurrence;
			const struct prototype_typing_motive_solution* motive_solution =
				&ctx->typing->constraints.motive_solution_for_projection[
					goal->owner_typed_projection
				];
			uint32_t motive = motive_solution->motive;
			if (motive == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			const struct prototype_typing_classifier_motive_case_goal* motive_case =
				&goal->payload.classifier.goal.motive_case;
			uint32_t actual_classifier = operation_solver_classifier_for_projection(
				ctx, motive_case->branch_typed_projection
			);
			uint32_t branch_context;
			uint32_t branch_substitution;
			int branch_context_status =
				operation_typed_projection_materialized_context(
					ctx,
					motive_case->branch_typed_projection,
					&branch_context,
					&branch_substitution
				);
			if (branch_context_status < 0) {
				return -1;
			}
			if (branch_context_status > 0) {
				return 0;
			}
			if (result->validated_motive == motive &&
				result->validated_actual_classifier == actual_classifier &&
				result->validated_context == branch_context &&
				result->validated_substitution == branch_substitution &&
				result->validated_recursive_revision ==
					motive_solution->recursive_equation_revision) {
				if (result->validation_status ==
					OPERATION_MOTIVE_EQUATION_VALID) {
					*p_reason =
						OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED;
					return 1;
				}
				return result->validation_status ==
						OPERATION_MOTIVE_EQUATION_INVALID ||
					result->validation_status ==
						OPERATION_MOTIVE_EQUATION_MISMATCH ? -1 : 0;
			}
			struct operation_motive_equation_validation validation;
			int validation_status = operation_motive_validate_equation(
				ctx,
				match_operation,
				goal->owner_typed_projection,
				goal->payload.classifier.goal.motive_case.case_index,
				motive,
				&validation
			);
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
				fprintf(
					stderr,
					"motive-validate constraint=%u status=%d motive=%u expected=%u "
					"actual=%u recursive=%d ih=%u guarded=%d conversion=%d\n",
					goal->id,
					validation_status,
					motive,
					validation.expected_classifier,
					validation.actual_classifier,
					validation.used_recursive_equation,
					validation.ih_operation,
					validation.guardedness_status,
					validation.conversion_status
				);
			}
			result->validated_motive = motive;
			result->validated_actual_classifier = actual_classifier;
			result->validated_context = branch_context;
			result->validated_substitution = branch_substitution;
			result->validated_recursive_revision =
				motive_solution->recursive_equation_revision;
			result->validation_status = validation_status;
			if (validation_status == OPERATION_MOTIVE_EQUATION_INVALID ||
				validation_status == OPERATION_MOTIVE_EQUATION_MISMATCH) {
				return -1;
			}
			if (validation_status != OPERATION_MOTIVE_EQUATION_VALID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_IH_EXPECTED:
		{
			if (goal->operand_count != 2) {
				return -1;
			}
			const struct prototype_typed_occurrence_graph* graph;
			struct prototype_typed_occurrence_induction_edge edge;
			graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
			int edge_status = prototype_typed_occurrence_graph_induction_edge(
					graph, ctx->terms, goal->source_occurrence, &edge
				);
			if (edge_status != 0) {
				return 0;
			}
			if (goal->payload.classifier.goal.induction_hypothesis.
					owner_match_occurrence != edge.owner_match_occurrence) {
				return 0;
			}
			if (goal->payload.classifier.goal.induction_hypothesis.ih_scope_id !=
					edge.scope_id) {
				return 0;
			}
			if (goal->payload.classifier.goal.induction_hypothesis.case_index !=
					edge.case_index) {
				return 0;
			}
			if (goal->payload.classifier.goal.induction_hypothesis.field_index !=
					edge.field_index) {
				return 0;
			}
			uint32_t recursive_classifier =
				operation_solver_classifier_for_projection(
				ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
			);
			if (recursive_classifier == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			if (goal->operand_count != 1) {
				return -1;
			}
			if (operation_solver_classifier_for_projection(
					ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_COMPUTATION_FOLD_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_OPERATION_REQUEST_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_TERMINATION_WITNESS:
			if (goal->operand_count != 1 ||
				operation_solver_classifier_for_projection(
					ctx, operation_solver_constraint_operand_projection(ctx, goal, 0)
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_TERMINATION_WITNESS_VALIDATED;
			return 1;
		default:
			return 0;
	}
}

static void operation_solver_refresh_constraint_states(
	struct compile_context* ctx,
	int incomplete
) {
	if (!ctx || !ctx->metadata) {
		return;
	}
	ctx->metadata->solver_constraint_count =
		prototype_typing_constraint_count_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
	ctx->metadata->solver_solved_count = 0;
	ctx->metadata->solver_residual_count = 0;
	ctx->metadata->solver_incomplete_count = 0;
	ctx->metadata->full_scan_count[PROTOTYPE_COMPILE_SCAN_CONSTRAINT]++;
	ctx->metadata->full_scan_visit_count[PROTOTYPE_COMPILE_SCAN_CONSTRAINT] +=
		prototype_typing_constraint_count_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
	for (uint32_t i = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		i != PROTOTYPE_INVALID_ID;
		i = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, i
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[i];
		struct prototype_typing_constraint_solution* result =
			&ctx->typing->constraints.solutions[i];
		if (result->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int residual = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			(result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_RESIDUAL ||
			 result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT);
		int goal_incomplete = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_EXHAUSTED;
		for (size_t obligation_id = 0;
			obligation_id <
				prototype_verification_db_count(&ctx->metadata->verification);
			++obligation_id) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(
					&ctx->metadata->verification, (uint32_t)obligation_id
				);
			if (!obligation) {
				continue;
			}
			if (obligation->occurrence == constraint->source_occurrence) {
				residual = 1;
				break;
			}
		}
		int solved_reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		int solved = operation_solver_classifier_goal_is_satisfied(
			ctx, constraint, &solved_reason
		);
		int state = OPERATION_CONSTRAINT_STATE_PENDING;
		int reason = OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY;
		if (incomplete || goal_incomplete) {
			state = OPERATION_CONSTRAINT_STATE_INCOMPLETE;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET;
		}
		if (solved > 0) {
			state = OPERATION_CONSTRAINT_STATE_SOLVED;
			reason = solved_reason;
		}
		if (residual) {
			state = OPERATION_CONSTRAINT_STATE_RESIDUAL;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION;
			if (constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE) {
				reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL;
			}
		}
		if (solved < 0) {
			state = OPERATION_CONSTRAINT_STATE_CONTRADICTION;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED;
		}
		if (prototype_typing_constraint_transition(
				&ctx->typing->constraints, i, state, reason
			) != 0) {
			return;
		}
		if (result->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (result->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (result->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		prototype_typing_constraint_count_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (incomplete &&
			solution->state == OPERATION_CONSTRAINT_STATE_PENDING) {
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					id,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (solution->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		prototype_typing_constraint_count_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_COMPUTATION
		);
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_COMPUTATION
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (incomplete && solution->state == OPERATION_CONSTRAINT_STATE_PENDING) {
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					id,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (solution->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		prototype_typing_constraint_count_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (incomplete &&
			solution->state == OPERATION_CONSTRAINT_STATE_PENDING) {
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					id,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (solution->state ==
			OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (solution->state ==
			OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (solution->state ==
			OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
}

static int operation_solver_propagate_zero_clause_computation_fold_input(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	uint32_t sequence_fold_operation_id = constraint ?
		constraint->source_occurrence : PROTOTYPE_INVALID_ID;
	if (!ctx || !ctx->metadata || !p_changed ||
		!constraint || (constraint->operand_count != 3 &&
			constraint->operand_count != 4) ||
		sequence_fold_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* sequence_fold_operation =
		&ctx->metadata->typed_occurrences.occurrences[sequence_fold_operation_id];
	uint32_t computation_occurrence;
	uint32_t continuation_occurrence_id;
	if (sequence_fold_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
		compile_occurrence_child(
			ctx, sequence_fold_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
			&computation_occurrence
		) != 0 || compile_occurrence_child(
			ctx, sequence_fold_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&continuation_occurrence_id
		) != 0 ||
		sequence_fold_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		ctx->terms->terms[sequence_fold_operation->core_term].as.computation_fold.clause_count != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier_for_projection(
		ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
	);
	const struct prototype_term* sequence_fold_term =
		&ctx->terms->terms[sequence_fold_operation->core_term];
	if (sequence_fold_term->as.computation_fold.return_clause >=
			ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		ctx->metadata->typed_occurrences.occurrences[
			continuation_occurrence_id
		].tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
		return -1;
	}
	uint32_t continuation_binder_classifier = PROTOTYPE_INVALID_ID;
	int continuation_binder_status =
		operation_solver_lambda_projection_binder_classifier(
			ctx,
			operation_solver_constraint_operand_projection(ctx, constraint, 1),
			&continuation_binder_classifier
		);
	if (continuation_binder_status < 0) {
		return -1;
	}
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			fprintf(
				stderr,
				"zero-fold-input-pending occurrence=%u computation=%u tag=%d "
				"projection=%u continuation-status=%d binder=%u\n",
				sequence_fold_operation_id,
				computation_occurrence,
				ctx->metadata->typed_occurrences.occurrences[
					computation_occurrence
				].tag,
				operation_solver_constraint_operand_projection(ctx, constraint, 0),
				continuation_binder_status,
				continuation_binder_classifier
			);
		}
		if (continuation_binder_status > 0) {
			return 0;
		}
		const struct prototype_typed_occurrence* computation_occurrence_entry =
			&ctx->metadata->typed_occurrences.occurrences[
				computation_occurrence
			];
		if (computation_occurrence_entry->tag !=
				PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			return 0;
		}
		int seed_status = operation_solver_seed_indexed_motive_from_ih_expected(
			ctx,
			computation_occurrence,
			operation_solver_constraint_operand_projection(ctx, constraint, 0),
			continuation_binder_classifier,
			p_changed
		);
		return seed_status < 0 ? -1 : 0;
	}
	const struct prototype_typed_occurrence* computation_entry =
		&ctx->metadata->typed_occurrences.occurrences[computation_occurrence];
	int ih_projection_current = 0;
	if (computation_entry->tag ==
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		const struct prototype_typing_constraint* ih_equation =
			operation_motive_ih_equation_for_projection_const(
				ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
			);
		if (!ih_equation || ih_equation->operand_count < 2) {
			return -1;
		}
		uint32_t owner_match_projection =
			operation_solver_constraint_operand_projection(ctx, ih_equation, 1);
		if (owner_match_projection >=
				ctx->typing->context_projections.typed_projection_count) {
			return -1;
		}
		uint32_t selected_motive = ctx->typing->constraints.
			motive_solution_for_projection[owner_match_projection].motive;
		if (selected_motive < ctx->terms->term_count) {
			ih_projection_current = operation_motive_ih_projection_is_current(
				ctx,
				operation_solver_constraint_operand_projection(ctx, constraint, 0),
				selected_motive
			);
		}
		if (selected_motive < ctx->terms->term_count &&
			!ih_projection_current) {
			/* A constant branch candidate may bootstrap motive synthesis, but once
			 * the owner has selected M it is no longer an answer for this recursive
			 * projection. Wait for M(argument) instead of letting the provisional
			 * candidate overwrite the sequence-result Context equation. */
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"zero-fold waiting for current IH occurrence=%u projection=%u "
					"motive=%u input=%u\n",
					sequence_fold_operation_id,
					operation_solver_constraint_operand_projection(ctx, constraint, 0),
					selected_motive,
					input_classifier
				);
			}
			return 0;
		}
	}
	struct prototype_classifier_view input_view;
	uint32_t input_value_classifier;
	int input_is_value =
		ctx->terms->terms[input_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE;
	if (input_is_value) {
		/* A recursive function field resolves to a suspended value after graph
		 * construction. The provisional fold is its implicit RETURN boundary. */
		input_value_classifier = input_classifier;
	} else {
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				input_classifier,
				&input_view
			) != 0) {
			return -1;
		}
		/* Other value-level synthesis frontiers remain pending until the motive
		 * determines whether they are returning computations. */
		if (input_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION) {
			return 0;
		}
		if (input_view.computation_kind !=
			PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		input_value_classifier = input_view.result;
	}
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
		uint32_t contextual_input = PROTOTYPE_INVALID_ID;
		int contextual_input_status = operation_solver_term_in_projection_context(
			ctx, constraint->owner_typed_projection, input_classifier,
			&contextual_input
		);
		struct prototype_term_conversion_result trace_conversion =
			continuation_binder_classifier == PROTOTYPE_INVALID_ID ?
				(struct prototype_term_conversion_result) { 0 } :
				prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					continuation_binder_classifier,
					input_value_classifier
				);
		fprintf(
			stderr,
			"zero-fold-input occurrence=%u owner-projection=%u input-projection=%u "
			"continuation-projection=%u input=%u input-value=%u binder=%u value=%d "
			"conversion=%d compatible=%d contextual=%d:%u\n",
			sequence_fold_operation_id,
			constraint->owner_typed_projection,
			operation_solver_constraint_operand_projection(ctx, constraint, 0),
			operation_solver_constraint_operand_projection(ctx, constraint, 1),
			input_classifier,
			input_value_classifier,
			continuation_binder_classifier,
			input_is_value,
			trace_conversion.status,
			continuation_binder_classifier != PROTOTYPE_INVALID_ID &&
				prototype_judgement_classifier_compatible(
					ctx->terms,
					ctx->type_declarations,
					continuation_binder_classifier,
					input_value_classifier
				),
			contextual_input_status,
			contextual_input_status == 0 ? contextual_input : PROTOTYPE_INVALID_ID
		);
	}
	if (computation_entry->tag ==
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
		!ih_projection_current &&
		continuation_binder_classifier != PROTOTYPE_INVALID_ID &&
		!prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			input_value_classifier,
			continuation_binder_classifier
		)) {
		int seed_status = operation_solver_seed_indexed_motive_from_ih_expected(
			ctx,
			computation_occurrence,
			operation_solver_constraint_operand_projection(ctx, constraint, 0),
			continuation_binder_classifier,
			p_changed
		);
		if (seed_status < 0) {
			return -1;
		}
		/* The recursive equation, rather than the provisional branch candidate,
		 * now owns the next IH classifier. Its materialization will requeue this
		 * fold with M(argument). */
		return 0;
	}
	if (continuation_binder_classifier != PROTOTYPE_INVALID_ID &&
		!prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			input_value_classifier,
			continuation_binder_classifier
		)) {
		uint32_t instantiated_input_classifier;
		int instantiate_status = prototype_judgement_solve_expected_effect_rows(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_value_classifier,
			continuation_binder_classifier,
			&instantiated_input_classifier
		);
		if (instantiate_status < 0) {
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"zero-fold expected-effect instantiation failed occurrence=%u "
					"input=%u binder=%u status=%d\n",
					sequence_fold_operation_id,
					input_value_classifier,
					continuation_binder_classifier,
					instantiate_status
				);
			}
			return -1;
		}
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			fprintf(
				stderr,
				"zero-fold expected-effect instantiation occurrence=%u input=%u "
				"binder=%u status=%d result=%u\n",
				sequence_fold_operation_id,
				input_value_classifier,
				continuation_binder_classifier,
				instantiate_status,
				instantiated_input_classifier
			);
		}
		if (instantiate_status == 0 &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				instantiated_input_classifier,
				continuation_binder_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			/* This is the fold edge's local specialization proof. The input
			 * projection keeps its own structural answer; replay validates the
			 * relation instead of rewriting that foreign equation. */
			input_value_classifier = instantiated_input_classifier;
		}
	}
	if (continuation_binder_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			continuation_binder_classifier,
			input_value_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	uint32_t continuation_classifier = operation_solver_classifier_for_projection(
		ctx, operation_solver_constraint_operand_projection(ctx, constraint, 1)
	);
	if (continuation_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t continuation_domain;
		uint32_t continuation_family;
		if (prototype_judgement_pi_parts(
				ctx->terms,
				continuation_classifier,
				&continuation_domain,
				&continuation_family
			) == 0 && !prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				continuation_domain,
				input_value_classifier
			)) {
			/* Refining the sequenced result changes the continuation binder before
			 * its Pi-formation constraint rebuilds the Lambda classifier. Defer this
			 * fold equation to that authoritative constraint instead of rejecting the
			 * stale Pi snapshot as a source typing error. */
			(void)continuation_family;
			return operation_solver_enqueue_dependents(
				ctx, continuation_occurrence_id
			);
		}
		uint32_t sequence_fold_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			ctx->terms,
			ctx->type_declarations,
			sequence_fold_term->as.computation_fold.computation,
			input_classifier,
			continuation_classifier,
			&sequence_fold_classifier
		);
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL && status != 0) {
			fprintf(
				stderr,
				"zero-fold-result waiting occurrence=%u status=%d input=%u "
				"continuation=%u binder=%u\n",
				sequence_fold_operation_id,
				status,
				input_classifier,
				continuation_classifier,
				continuation_binder_classifier
			);
		}
		if (status < 0 || (status == 0 && operation_solver_publish_projection(
				ctx, constraint->owner_typed_projection, sequence_fold_classifier,
				p_changed
			) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_projected_fold_clause(
	struct compile_context* ctx,
	uint32_t fold_typed_projection,
	uint32_t clause_index,
	uint32_t* p_operation_projection,
	uint32_t* p_outer_lambda_projection,
	uint32_t* p_inner_lambda_projection,
	uint32_t* p_body_projection
) {
	if (!ctx || !p_operation_projection || !p_outer_lambda_projection ||
		!p_inner_lambda_projection || !p_body_projection ||
		operation_solver_projected_child(
			ctx, fold_typed_projection,
			PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, clause_index,
			p_operation_projection
		) != 0 || operation_solver_projected_child(
			ctx, fold_typed_projection,
			PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, clause_index,
			p_outer_lambda_projection
		) != 0 || operation_solver_projected_child(
			ctx, *p_outer_lambda_projection,
			PROTOTYPE_TERM_CHILD_BODY, 0,
			p_inner_lambda_projection
		) != 0 || operation_solver_projected_child(
			ctx, *p_inner_lambda_projection,
			PROTOTYPE_TERM_CHILD_BODY, 0,
			p_body_projection
		) != 0) {
		return -1;
	}
	return 0;
}

static int operation_solver_propagate_clause_computation_fold_input(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	uint32_t handle_operation_id = constraint ?
		constraint->source_occurrence : PROTOTYPE_INVALID_ID;
	if (!ctx || !ctx->metadata || !p_changed ||
		!constraint || (constraint->operand_count != 3 &&
			constraint->operand_count != 4) ||
		handle_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* handle_operation =
		&ctx->metadata->typed_occurrences.occurrences[handle_operation_id];
	uint32_t computation_occurrence;
	uint32_t return_operation;
	if (handle_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
		compile_occurrence_child(
			ctx, handle_operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION,
			0, &computation_occurrence
		) != 0 || compile_occurrence_child(
			ctx, handle_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, &return_operation
		) != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier_for_projection(
		ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0) {
		return -1;
	}
	/* Keep provisional motive applications pending until the IH/motive
	 * equations close them as computations. */
	if (input_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION) {
		return 0;
	}
	if (
		input_view.computation_kind != PROTOTYPE_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	const struct prototype_typed_occurrence* return_lambda =
		&ctx->metadata->typed_occurrences.occurrences[return_operation];
	if (return_lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		return_lambda->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	int return_binder_status =
		operation_solver_bind_lambda_projection_binder_classifier(
			ctx,
			operation_solver_constraint_operand_projection(ctx, constraint, 1),
			input_view.result,
			1,
			p_changed
		);
	if (return_binder_status < 0) {
		return -1;
	}
	if (return_binder_status > 0) {
		return 0;
	}
	if (handle_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[handle_operation->core_term].tag !=
			PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold_term =
		&ctx->terms->terms[handle_operation->core_term];
	if ((size_t)fold_term->as.computation_fold.first_clause +
			fold_term->as.computation_fold.clause_count >
			ctx->terms->computation_fold_clause_count ||
		handle_operation->fold_clause_count !=
			fold_term->as.computation_fold.clause_count ||
		(size_t)handle_operation->first_fold_clause +
			handle_operation->fold_clause_count >
			ctx->metadata->typed_occurrences.fold_clause_count) {
		return -1;
	}
	uint32_t return_body_classifier = operation_solver_classifier_for_projection(
		ctx, operation_solver_constraint_operand_projection(ctx, constraint, 2)
	);
	struct prototype_classifier_view return_body_view;
	if (return_body_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			return_body_classifier,
			&return_body_view
		) != 0 || return_body_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
		return_body_view.computation_kind !=
			PROTOTYPE_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	uint32_t fold_effect_row = return_body_view.effect_row;
	int fold_totality = prototype_classifier_totality_join(
		input_view.totality, return_body_view.totality
	);
	if (fold_totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
		return 0;
	}
	int clause_bodies_complete = 1;
	uint32_t handled_row;
	if (prototype_term_effect_row_empty(ctx->terms, &handled_row
		) != 0) {
		return -1;
	}
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_typed_occurrence_fold_clause* occurrence_clause =
			&ctx->metadata->typed_occurrences.fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		uint32_t clause_operation_occurrence;
		uint32_t clause_lambda_occurrence;
		uint32_t clause_body_occurrence;
		uint32_t clause_operation_projection;
		uint32_t clause_outer_lambda_projection;
		uint32_t clause_inner_lambda_projection;
		uint32_t clause_body_projection;
		if (compile_fold_clause_occurrences(
				ctx, handle_operation_id, clause_index,
				&clause_operation_occurrence, &clause_lambda_occurrence,
				&clause_body_occurrence
			) != 0 || operation_solver_projected_fold_clause(
				ctx, constraint->owner_typed_projection, clause_index,
				&clause_operation_projection, &clause_outer_lambda_projection,
				&clause_inner_lambda_projection, &clause_body_projection
			) != 0) {
			return -1;
		}
		(void)occurrence_clause;
		(void)clause_operation_occurrence;
		(void)clause_lambda_occurrence;
		(void)clause_operation_projection;
		(void)clause_outer_lambda_projection;
		(void)clause_inner_lambda_projection;
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_core_effect_operation_declaration* declaration =
			prototype_core_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		uint32_t empty_row;
		uint32_t operation_atom;
		uint32_t next_handled_row;
		if (prototype_term_effect_row_empty(ctx->terms, &empty_row
			) != 0 || prototype_term_effect_row_operation(
				ctx->terms, operation_identity, empty_row, &operation_atom
			) != 0 || prototype_term_effect_row_union(
				ctx->terms, handled_row, operation_atom, &next_handled_row
			) != 0) {
			return -1;
		}
		handled_row = next_handled_row;
		uint32_t clause_body_classifier = operation_solver_classifier_for_projection(
			ctx, clause_body_projection
		);
		if (clause_body_classifier == PROTOTYPE_INVALID_ID) {
			clause_bodies_complete = 0;
			continue;
		}
		struct prototype_classifier_view clause_body_view;
		uint32_t joined_row;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				clause_body_classifier,
				&clause_body_view
			) != 0 || clause_body_view.category !=
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			clause_body_view.computation_kind !=
				PROTOTYPE_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_union(
				ctx->terms,
				fold_effect_row,
				clause_body_view.effect_row,
				&joined_row
			) != 0) {
			return -1;
		}
		fold_totality = prototype_classifier_totality_join(
			fold_totality, clause_body_view.totality
		);
		if (fold_totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
			clause_bodies_complete = 0;
			continue;
		}
		fold_effect_row = joined_row;
	}
	uint32_t residual_row;
	uint32_t joined_row;
	int residual_status = prototype_term_effect_row_residual(
		ctx->terms, input_view.effect_row, handled_row, &residual_row
	);
	if (residual_status < 0 || (residual_status == 0 &&
		prototype_term_effect_row_union(
			ctx->terms, fold_effect_row, residual_row, &joined_row
		) != 0)) {
		return -1;
	}
	if (residual_status == 0) {
		fold_effect_row = joined_row;
	}
	uint32_t fold_output_classifier;
	if (prototype_term_computation_type(
			ctx->terms,
			fold_effect_row,
			return_body_view.result,
			fold_totality,
			&fold_output_classifier
		) != 0) {
		return -1;
	}
	/* A symbolic clause row is precisely the case in which the operation
	 * domain must still be propagated into the clause binders below.  Defer
	 * publishing the fold result, but do not return before that propagation. */
	if (clause_bodies_complete &&
		prototype_term_effect_row_is_closed(ctx->terms, fold_effect_row) == 1) {
		if (operation_solver_widen_computation_binding(
				ctx, handle_operation_id, fold_output_classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_typed_occurrence_fold_clause* occurrence_clause =
			&ctx->metadata->typed_occurrences.fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		uint32_t clause_operation_occurrence;
		uint32_t clause_lambda_occurrence;
		uint32_t clause_body_occurrence;
		uint32_t clause_operation_projection;
		uint32_t clause_outer_lambda_projection;
		uint32_t clause_inner_lambda_projection;
		uint32_t clause_body_projection;
		if (compile_fold_clause_occurrences(
				ctx, handle_operation_id, clause_index,
				&clause_operation_occurrence, &clause_lambda_occurrence,
				&clause_body_occurrence
			) != 0 || operation_solver_projected_fold_clause(
				ctx, constraint->owner_typed_projection, clause_index,
				&clause_operation_projection, &clause_outer_lambda_projection,
				&clause_inner_lambda_projection, &clause_body_projection
			) != 0) {
			return -1;
		}
		(void)clause_body_occurrence;
		(void)clause_outer_lambda_projection;
		(void)clause_inner_lambda_projection;
		(void)clause_body_projection;
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_core_effect_operation_declaration* declaration =
			prototype_core_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		if (declaration->resumption_multiplicity !=
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT) {
			const struct prototype_typed_occurrence* outer_clause =
				&ctx->metadata->typed_occurrences.occurrences[clause_lambda_occurrence];
			uint32_t continuation_clause_occurrence;
			if (outer_clause->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				compile_occurrence_child(
					ctx, clause_lambda_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
					&continuation_clause_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence* continuation_clause =
				&ctx->metadata->typed_occurrences.occurrences[continuation_clause_occurrence];
			const struct prototype_occurrence_usage_solution* continuation_usage =
				&ctx->usage_workspace.solutions[continuation_clause_occurrence];
			if (continuation_clause->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				continuation_clause->binding_id !=
					occurrence_clause->continuation_binder_id ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE &&
					continuation_usage->binder_usage !=
						PROTOTYPE_USAGE_ZERO) ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
					continuation_usage->binder_usage ==
						PROTOTYPE_USAGE_MANY)) {
				return -1;
			}
		}
		uint32_t outer_lambda = core_clause->body;
		if (outer_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[outer_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t inner_lambda = ctx->terms->terms[outer_lambda].as.lambda.body;
		if (inner_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[inner_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t operation_classifier = operation_solver_classifier_for_projection(
			ctx, clause_operation_projection
		);
		uint32_t operation_classifier_whnf;
		uint32_t operation_domain;
		uint32_t operation_codomain_family;
		if (operation_classifier == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		int specialization_status =
			prototype_judgement_specialize_fold_operation_classifier(
				ctx->terms,
				ctx->type_declarations,
				input_view.effect_row,
				operation_identity,
				operation_classifier,
				&operation_classifier
			);
		if (specialization_status < 0) {
			return -1;
		}
		/* A symbolic input row cannot yet specialize a higher-order operation's
		 * delayed-computation domain. Its generic Pi still determines the request
		 * result and therefore the resumption binder. Propagate that classifier
		 * now; a later fold pass may replace the row-polymorphic domain with the
		 * specialized one. */
		if (typing_core_normalize_complete(
			ctx->core_requests,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			operation_classifier,
			&operation_classifier_whnf
			) != 0) {
			return -1;
		}
		for (uint32_t depth = 0;
			depth < 32 && operation_classifier_whnf < ctx->terms->term_count &&
			ctx->terms->terms[operation_classifier_whnf].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_FORALL;
			++depth) {
			operation_classifier_whnf = ctx->terms->terms[
				operation_classifier_whnf
			].as.effect_row_forall.body;
			if (typing_core_normalize_complete(
				ctx->core_requests,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				operation_classifier_whnf,
				&operation_classifier_whnf
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_pi_parts(
				ctx->terms,
				operation_classifier_whnf,
				&operation_domain,
				&operation_codomain_family
			) != 0) {
			return -1;
		}
		uint32_t operation_result_binder;
		uint32_t operation_result_computation;
		struct prototype_classifier_view operation_result_view;
		if (
			prototype_term_pure_family_parts(
				ctx->terms,
				operation_codomain_family,
				&operation_result_binder,
				&operation_result_computation
			) != 0 || prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				operation_result_computation,
				&operation_result_view
			) != 0 || operation_result_view.category !=
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			operation_result_view.computation_kind !=
				PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			return 0;
		}
		(void)operation_result_binder;
		uint32_t continuation_function_classifier;
		uint32_t continuation_binder_classifier;
		if (prototype_term_pi(
				ctx->terms,
				operation_result_view.result,
				fold_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				ctx->terms,
				continuation_function_classifier,
				&continuation_binder_classifier
			) != 0) {
			return -1;
		}
		/* Binder classifiers belong to the projected clause context. Publishing by
		 * bare occurrence ID loses that context when a core Lambda is shared. */
		int argument_binder_status =
			operation_solver_bind_lambda_projection_binder_classifier(
				ctx, clause_outer_lambda_projection, operation_domain, 1, p_changed
			);
		if (argument_binder_status < 0) {
			return -1;
		}
		if (argument_binder_status > 0) {
			return 0;
		}
		int continuation_binder_status =
			operation_solver_bind_lambda_projection_binder_classifier(
				ctx, clause_inner_lambda_projection,
				continuation_binder_classifier, 1, p_changed
			);
		if (continuation_binder_status < 0) {
			return -1;
		}
		if (continuation_binder_status > 0) {
			return 0;
		}
	}
	return 0;
}

static int operation_solver_occurrence_topology_grew(
	const struct compile_context* ctx
) {
	return ctx && ctx->classifier_workspace.initialized_occurrence_count !=
		ctx->metadata->typed_occurrences.occurrence_count;
}

enum operation_conversion_validation {
	OPERATION_CONVERSION_INVALID = -1,
	OPERATION_CONVERSION_WAITING = 0,
	OPERATION_CONVERSION_EQUAL = 1,
	OPERATION_CONVERSION_COMPATIBLE = 2,
	OPERATION_CONVERSION_REJECTED = 3,
	OPERATION_CONVERSION_EFFORT_EXHAUSTED = 4
};

static int operation_solver_validate_conversion(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	struct prototype_typing_constraint_solution* solution,
	uint32_t conversion_context,
	uint32_t expected_classifier,
	uint32_t actual_classifier,
	uint32_t* p_checked_expected
) {
	if (!ctx || !constraint || !solution || !p_checked_expected) {
		return OPERATION_CONVERSION_INVALID;
	}
	uint32_t checked_expected = expected_classifier;
	int effect_status = prototype_judgement_solve_expected_effect_rows(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		expected_classifier,
		actual_classifier,
		&checked_expected
	);
	if (effect_status < 0) {
		return OPERATION_CONVERSION_INVALID;
	}
	*p_checked_expected = checked_expected;
	if (effect_status == 0) {
		solution->conversion_goal.id = constraint->id;
		solution->conversion_goal.context_id = conversion_context;
		solution->conversion_goal.carrier_classifier = PROTOTYPE_INVALID_ID;
		solution->conversion_goal.left_term = checked_expected;
		solution->conversion_goal.right_term = actual_classifier;
		solution->conversion_goal.normalization_profile =
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF;
		solution->conversion_goal.step_limit =
			compile_effort_remaining(ctx->metadata);
		int effort_status = compile_effort_charge(
			ctx->metadata,
			ctx->effort_atomic,
			PROTOTYPE_EFFORT_PHASE_NORMALIZATION,
			1
		);
		if (effort_status != 0) {
			return effort_status > 0 ?
				OPERATION_CONVERSION_EFFORT_EXHAUSTED : OPERATION_CONVERSION_INVALID;
		}
		if (prototype_judgement_kernel_conversion_goal_execute(
				&ctx->metadata->contexts,
				ctx->terms,
				ctx->type_declarations,
				NULL,
				&solution->conversion_goal,
				0
			) != 0) {
			return OPERATION_CONVERSION_INVALID;
		}
		switch (solution->conversion_goal.result.status) {
			case PROTOTYPE_TERM_CONVERSION_EQUAL:
				return OPERATION_CONVERSION_EQUAL;
			case PROTOTYPE_TERM_CONVERSION_NOT_EQUAL:
				break;
			case PROTOTYPE_TERM_CONVERSION_RESIDUAL:
			case PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT:
			case PROTOTYPE_TERM_CONVERSION_EXHAUSTED:
				return OPERATION_CONVERSION_WAITING;
			default:
				return OPERATION_CONVERSION_INVALID;
		}
	}

	if (prototype_judgement_classifier_compatible(
			ctx->terms,
			ctx->type_declarations,
			checked_expected,
			actual_classifier
		)) {
		return OPERATION_CONVERSION_COMPATIBLE;
	}
	int structural_status = operation_check_classifier(
		ctx,
		constraint->payload.classifier.goal.conversion.body_occurrence,
		checked_expected,
		0
	);
	if (structural_status < 0) {
		return OPERATION_CONVERSION_INVALID;
	}
	if (structural_status > 0) {
		int unowned_row = operation_classifier_contains_unowned_effect_row(
			ctx,
			constraint->source_occurrence,
			checked_expected
		);
		if (unowned_row < 0) {
			return OPERATION_CONVERSION_INVALID;
		}
		if (unowned_row > 0) {
			return OPERATION_CONVERSION_REJECTED;
		}
		return OPERATION_CONVERSION_COMPATIBLE;
	}
	return OPERATION_CONVERSION_REJECTED;
}

static int operation_resolve_source_classifier_rhs_endpoint(
	void* context,
	uint32_t source_occurrence,
	uint32_t* p_term
) {
	struct compile_context* ctx = context;
	if (!ctx || !p_term || source_occurrence >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t term = ctx->metadata->typed_occurrences.occurrences[
		source_occurrence
	].core_term;
	if (term >= ctx->terms->term_count) {
		return -1;
	}
	*p_term = term;
	return 0;
}

static int operation_solver_classifier_rhs_for_projection(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* equation,
	uint32_t typed_projection,
	uint32_t* p_classifier
) {
	if (!ctx || !equation || !p_classifier) {
		return -1;
	}
	if (equation->classifier_rhs_root == PROTOTYPE_INVALID_ID) {
		return operation_solver_lambda_projection_binder_classifier(
			ctx, typed_projection, p_classifier
		);
	}
	struct operation_classifier_rhs_evaluator evaluator = {
		.endpoint_context = ctx,
		.resolve_endpoint = operation_resolve_source_classifier_rhs_endpoint
	};
	uint32_t source_classifier;
	int status = operation_evaluate_classifier_rhs(
		ctx,
		equation->classifier_rhs_root,
		&evaluator,
		&source_classifier
	);
	if (status != 0) {
		return status;
	}
	return operation_solver_term_in_projection_context(
		ctx, typed_projection, source_classifier, p_classifier
	);
}

static int operation_solver_solve_pi_intro(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	if (!ctx || !constraint || !p_changed || constraint->operand_count != 1 ||
		constraint->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* lambda =
		&ctx->metadata->typed_occurrences.occurrences[
			constraint->source_occurrence
		];
	if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		lambda->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[lambda->core_term].tag != PROTOTYPE_TERM_LAMBDA ||
		lambda->binding_id == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "Pi intro: invalid Lambda equation=%u\n", constraint->id);
		return -1;
	}

	uint32_t body_projection = operation_solver_constraint_operand_projection(ctx, constraint, 0);
	uint32_t lambda_classifier = operation_solver_classifier_for_projection(
		ctx, constraint->owner_typed_projection
	);
	uint32_t body_classifier = operation_solver_classifier_for_projection(
		ctx, body_projection
	);
	uint32_t domain = PROTOTYPE_INVALID_ID;
	uint32_t codomain_family = PROTOTYPE_INVALID_ID;
	int pi_status = lambda_classifier == PROTOTYPE_INVALID_ID ? 1 :
		prototype_judgement_pi_parts(
			ctx->terms, lambda_classifier, &domain, &codomain_family
		);

	if (pi_status == 0) {
		uint32_t current_domain;
		int domain_status = operation_solver_classifier_rhs_for_projection(
			ctx, constraint, constraint->owner_typed_projection, &current_domain
		);
		if (domain_status < 0) {
			fprintf(stderr, "Pi intro: binder lookup failed equation=%u\n",
				constraint->id);
			return -1;
		}
		if (domain_status > 0) {
			uint32_t sequence_domain;
			int sequence_status =
				operation_solver_projected_sequence_binder_classifier(
					ctx,
					constraint->owner_typed_projection,
					&sequence_domain
				);
			if (sequence_status < 0) {
				return -1;
			}
			if (sequence_status == 2) {
				return 0;
			}
			int bind_status =
				operation_solver_bind_lambda_projection_binder_classifier(
					ctx, constraint->owner_typed_projection, domain, 0, p_changed
				);
			if (bind_status < 0) {
				fprintf(stderr, "Pi intro: binder publication failed equation=%u\n",
					constraint->id);
				return -1;
			}
			if (bind_status > 0) {
				return 0;
			}
			current_domain = domain;
		}
		if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, domain, current_domain
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			uint32_t ignored_body_projection;
			uint32_t binding_context;
			int context_status = operation_solver_lambda_projection_binding_context(
				ctx,
				constraint->owner_typed_projection,
				&ignored_body_projection,
				&binding_context
			);
			if (context_status < 0) {
				fprintf(stderr, "Pi intro: binder Context lookup failed equation=%u\n",
					constraint->id);
				return -1;
			}
			if (context_status > 0) {
				return 0;
			}
			if (!operation_solver_binding_classifier_is_inferred(
					ctx, lambda->binding_id, binding_context
				)) {
				fprintf(stderr, "Pi intro: binder classifier mismatch equation=%u\n",
					constraint->id);
				return -1;
			}
			/* A generated binder's Context equation is authoritative. The existing
			 * Lambda classifier is a stale derived answer and is rebuilt below. */
			domain = current_domain;
		}

		uint32_t codomain_binder;
		uint32_t codomain_body;
		uint32_t binder_var;
		uint32_t specialized_body;
		if (prototype_term_pure_family_parts(
				ctx->terms, codomain_family,
				&codomain_binder, &codomain_body
			) != 0) {
			fprintf(stderr, "Pi intro: expected codomain is not a family equation=%u\n",
				constraint->id);
			return -1;
		}
		if (prototype_term_var(
				ctx->terms, lambda->binding_id, &binder_var
			) != 0) {
			fprintf(stderr, "Pi intro: binder term construction failed equation=%u\n",
				constraint->id);
			return -1;
		}
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms, codomain_body, codomain_binder,
				binder_var, &specialized_body
			) != 0) {
			fprintf(stderr, "Pi intro: codomain specialization failed equation=%u\n",
				constraint->id);
			return -1;
		}
		int refine_status = operation_refine_projected_classifier_from_expected(
			ctx,
			body_projection,
			specialized_body,
			constraint->id,
			0,
			p_changed
		);
		if (refine_status < 0) {
			fprintf(stderr, "Pi intro: body refinement failed equation=%u\n",
				constraint->id);
			return -1;
		}
		body_classifier = operation_solver_classifier_for_projection(
			ctx, body_projection
		);
		if (body_classifier == PROTOTYPE_INVALID_ID) {
			if (operation_solver_publish_projection(
					ctx, body_projection, specialized_body, p_changed
				) != 0) {
				fprintf(stderr, "Pi intro: body publication failed equation=%u\n",
					constraint->id);
				return -1;
			}
			body_classifier = specialized_body;
		}
	}

	if (body_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	int domain_status = operation_solver_classifier_rhs_for_projection(
		ctx, constraint, constraint->owner_typed_projection, &domain
	);
	if (domain_status < 0) {
		fprintf(stderr, "Pi intro: final binder lookup failed equation=%u\n",
			constraint->id);
		return -1;
	}
	if (domain_status > 0) {
		return 0;
	}
	if (operation_classifier_lambda_intro_result(
			ctx,
			constraint->source_occurrence,
			domain,
			body_classifier,
			&lambda_classifier
		) != 0) {
		fprintf(stderr, "Pi intro: classifier construction failed equation=%u\n",
			constraint->id);
		return -1;
	}
	if (operation_solver_publish_projection(
			ctx, constraint->owner_typed_projection,
			lambda_classifier, p_changed
		) != 0) {
		fprintf(stderr, "Pi intro: classifier publication failed equation=%u\n",
			constraint->id);
		return -1;
	}
	return 0;
}

static int operation_solver_solve_pi_domain_relation(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	if (!ctx || !constraint || !p_changed || constraint->operand_count != 1) {
		return -1;
	}
	uint32_t function_projection =
		operation_solver_constraint_operand_projection(ctx, constraint, 0);
	uint32_t function_classifier = operation_solver_classifier_for_projection(
		ctx, function_projection
	);
	uint32_t argument_classifier = operation_solver_classifier_for_projection(
		ctx, constraint->owner_typed_projection
	);
	if (function_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t application_projection = constraint->payload.classifier.goal.
		pi_domain_relation.application_typed_projection;
	const struct prototype_typed_projection* application =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, application_projection
		);
	const struct prototype_typed_projection* argument =
		prototype_typed_projection_get(
			&ctx->typing->context_projections,
			constraint->owner_typed_projection
		);
	if (!application || !argument || application->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		argument->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* application_occurrence =
		&ctx->metadata->typed_occurrences.occurrences[
			application->source_occurrence
		];
	const struct prototype_typed_occurrence* argument_occurrence =
		&ctx->metadata->typed_occurrences.occurrences[
			argument->source_occurrence
		];
	if (application_occurrence->application_role ==
			PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION) {
		uint32_t contextual_function;
		int context_status = operation_solver_term_in_projection_context(
			ctx, application_projection, function_classifier,
			&contextual_function
		);
		if (context_status < 0) {
			return -1;
		}
		if (context_status > 0) {
			return 0;
		}
		uint32_t expected_domain;
		int domain_status = operation_callable_classifier_domain(
			ctx, contextual_function, &expected_domain
		);
		if (domain_status < 0) {
			return -1;
		}
		if (domain_status > 0) {
			return 0;
		}
		int refinement_changed = 0;
		int refine_status = operation_refine_projected_classifier_from_expected(
			ctx,
			constraint->owner_typed_projection,
			expected_domain,
			constraint->id,
			0,
			&refinement_changed
		);
		if (refine_status < 0) {
			return -1;
		}
		*p_changed |= refinement_changed;
		if (refinement_changed) {
			return 0;
		}
		argument_classifier = operation_solver_classifier_for_projection(
			ctx, constraint->owner_typed_projection
		);
	}
	if (argument_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t contextual_function;
	uint32_t contextual_argument_classifier;
	uint32_t contextual_argument_term;
	int context_status = operation_solver_application_inputs_in_projection_context(
		ctx,
		application_projection,
		function_classifier,
		argument_classifier,
		argument_occurrence->core_term,
		&contextual_function,
		&contextual_argument_classifier,
		&contextual_argument_term
	);
	if (context_status < 0) return -1;
	if (context_status > 0) return 0;
	int relation_status;
	if (application_occurrence->application_role ==
			PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION) {
		uint32_t ignored_result;
		uint32_t application_context;
		uint32_t application_substitution;
		if (operation_typed_projection_materialized_context(
				ctx,
				application_projection,
				&application_context,
				&application_substitution
			) != 0) {
			return 0;
		}
		(void)application_substitution;
		relation_status = prototype_judgement_static_family_app_classifier(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			application_context,
			contextual_function,
			contextual_argument_term,
			contextual_argument_classifier,
			&ignored_result
		);
	} else {
		uint32_t specialized;
		uint32_t pi;
		uint32_t expected_domain;
		relation_status = operation_callable_classifier_parts(
			ctx,
			contextual_function,
			contextual_argument_classifier,
			contextual_argument_term,
			0,
			&specialized,
			&pi,
			&expected_domain
		);
		if (relation_status == 0) {
			uint32_t solved_expected;
			relation_status = prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				expected_domain,
				contextual_argument_classifier,
				&solved_expected
			);
		}
		(void)specialized;
		(void)pi;
	}
	if (relation_status < 0) {
		return -1;
	}
	if (relation_status > 0) {
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[constraint->id];
		solution->contradiction_actual_classifier = argument_classifier;
		if (prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				constraint->id,
				OPERATION_CONSTRAINT_STATE_CONTRADICTION,
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_DOMAIN_CONTRADICTION
			) != 0) {
			return -1;
		}
		return 0;
	}
	/* A relation equation validates the structural answer selected by its owner.
	 * It records that answer only in its own cell and never publishes a competing
	 * classifier to the owner's aggregate equation. */
	return operation_solver_equation_candidate_transition(
		ctx,
		constraint->owner_typed_projection,
		constraint->id,
		argument_classifier,
		p_changed
	);
}

static int operation_solver_materialize_sequence_binder_context_view(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* equation,
	uint32_t classifier,
	int* p_changed
);

static int operation_solver_register_sequence_binder_context(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* equation,
	uint32_t* p_equation_id
) {
	uint32_t ignored_body;
	uint32_t binding_context;
	if (!ctx || !equation || !p_equation_id || equation->kind !=
			OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION) {
		return -1;
	}
	int binding_status = operation_solver_lambda_projection_binding_context(
		ctx,
		equation->owner_typed_projection,
		&ignored_body,
		&binding_context
	);
	if (binding_status != 0) {
		return binding_status;
	}
	const struct prototype_context* binding = prototype_context_get(
		&ctx->metadata->contexts, binding_context
	);
	if (!binding) {
		return -1;
	}
	uint32_t equation_id = prototype_context_classifier_equation(binding);
	if (equation_id >= PROTOTYPE_CONTEXT_CAPACITY) {
		return -1;
	}
	uint32_t* registered = &ctx->typing->constraints.
		sequence_relation_for_context_equation[equation_id];
	if (*registered != PROTOTYPE_INVALID_ID && *registered != equation->id) {
		return -1;
	}
	*registered = equation->id;
	*p_equation_id = equation_id;
	return 0;
}

static int operation_solver_solve_sequence_binder_relation(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	if (!ctx || !constraint || !p_changed ||
		(constraint->operand_count != 1 && constraint->operand_count != 2)) {
		return -1;
	}
	uint32_t binding_equation;
	int binding_status = operation_solver_register_sequence_binder_context(
		ctx, constraint, &binding_equation
	);
	if (binding_status < 0) {
		return -1;
	}
	if (binding_status > 0) {
		return 0;
	}
	const struct prototype_context_classifier_equation* context_equation =
		prototype_context_classifier_equation_get(
			&ctx->metadata->contexts, binding_equation
		);
	const struct prototype_context_classifier_equation* source_equation =
		context_equation;
	if (source_equation && source_equation->key_kind ==
			CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		source_equation = prototype_context_classifier_equation_get(
			&ctx->metadata->contexts, source_equation->key_value
		);
	}
	uint32_t ignored_body_projection;
	uint32_t binding_context;
	if (!context_equation || !source_equation ||
		operation_solver_lambda_projection_binding_context(
			ctx, constraint->owner_typed_projection,
			&ignored_body_projection, &binding_context
		) != 0) {
		return -1;
	}
	uint32_t fixed_value_classifier =
		source_equation->source_classifier_authority ==
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED ?
		operation_context_classifier_term(ctx, binding_context) :
		PROTOTYPE_INVALID_ID;
	uint32_t producer_projection =
		operation_solver_constraint_operand_projection(ctx, constraint, 0);
	const struct prototype_typed_projection* producer =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, producer_projection
		);
	if (!producer || producer->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* producer_occurrence =
		&ctx->metadata->typed_occurrences.occurrences[
			producer->source_occurrence
		];
	uint32_t producer_classifier = operation_solver_classifier_for_projection(
		ctx, producer_projection
	);
	if (producer_classifier == PROTOTYPE_INVALID_ID) {
		if (producer_occurrence->tag !=
				PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			return 0;
		}
		uint32_t expected = ctx->typing->constraints.solutions[
			constraint->id
		].result_term;
		if (expected == PROTOTYPE_INVALID_ID) {
			uint32_t ignored_body;
			uint32_t binding_context;
			int binding_status =
				operation_solver_lambda_projection_binding_context(
					ctx, constraint->owner_typed_projection, &ignored_body,
					&binding_context
				);
			if (binding_status < 0) {
				return -1;
			}
			if (binding_status > 0) {
				return 0;
			}
			expected = operation_context_classifier_term(ctx, binding_context);
			if (expected == PROTOTYPE_INVALID_ID) {
				return 0;
			}
		}
		int seed_status = operation_solver_seed_indexed_motive_from_ih_expected(
			ctx,
			producer->source_occurrence,
			producer_projection,
			expected,
			p_changed
		);
		return seed_status < 0 ? -1 : 0;
	}
	if (producer_occurrence->tag ==
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		const struct prototype_typing_constraint* ih_equation =
			operation_motive_ih_equation_for_projection_const(
				ctx, producer_projection
			);
		if (!ih_equation || ih_equation->operand_count < 2) {
			return -1;
		}
		uint32_t owner_match_projection =
			operation_solver_constraint_operand_projection(ctx, ih_equation, 1);
		if (owner_match_projection >=
				ctx->typing->context_projections.typed_projection_count) {
			return -1;
		}
		uint32_t selected_motive = ctx->typing->constraints.
			motive_solution_for_projection[owner_match_projection].motive;
		if (selected_motive < ctx->terms->term_count &&
			!operation_motive_ih_projection_is_current(
				ctx, producer_projection, selected_motive
			)) {
			return 0;
		}
	}
	uint32_t value_classifier;
	if (producer_classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->terms->terms[producer_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		value_classifier = producer_classifier;
	} else {
		struct prototype_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				producer_classifier,
				&view
			) != 0) {
			return -1;
		}
		if (view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION) {
			return 0;
		}
		if (view.computation_kind != PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		value_classifier = view.result;
	}
	uint32_t projected_classifier;
	int projection_status = operation_solver_term_in_projection_context(
		ctx,
		constraint->owner_typed_projection,
		value_classifier,
		&projected_classifier
	);
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
		fprintf(
			stderr,
			"sequence-binder-relation constraint=%u producer=%u "
			"producer-classifier=%u value-classifier=%u projection-status=%d "
			"projected=%u\n",
			constraint->id,
			producer_projection,
			producer_classifier,
			value_classifier,
			projection_status,
			projection_status == 0 ? projected_classifier : PROTOTYPE_INVALID_ID
		);
	}
	if (projection_status != 0) {
		return projection_status < 0 ? -1 : 0;
	}
	if (fixed_value_classifier != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			projected_classifier,
			fixed_value_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		uint32_t expected_producer = fixed_value_classifier;
		if (ctx->terms->terms[producer_classifier].tag !=
				PROTOTYPE_TERM_THUNK_TYPE) {
			struct prototype_classifier_view producer_view;
			if (prototype_judgement_classifier_view(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					producer_classifier,
					&producer_view
				) != 0 || producer_view.category !=
					PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
				producer_view.computation_kind !=
					PROTOTYPE_COMPUTATION_KIND_RETURNING ||
				prototype_term_computation_type(
					ctx->terms,
					producer_view.effect_row,
					fixed_value_classifier,
					producer_view.totality,
					&expected_producer
				) != 0) {
				return -1;
			}
		}
		int refine_changed = 0;
		int refine_status = operation_refine_projected_classifier_from_expected(
			ctx,
			producer_projection,
			expected_producer,
			constraint->id,
			0,
			&refine_changed
		);
		if (refine_status < 0) {
			return -1;
		}
		if (refine_changed) {
			*p_changed = 1;
		}
		/* A fixed source annotation is the equation's authority.  Until the
		 * producer projects to that value, the relation stays pending instead of
		 * publishing a provisional producer answer back into ContextDB. */
		return 0;
	}
	if (fixed_value_classifier != PROTOTYPE_INVALID_ID) {
		projected_classifier = fixed_value_classifier;
	}
	int relation_status = operation_solver_equation_candidate_transition(
		ctx,
		constraint->owner_typed_projection,
		constraint->id,
		projected_classifier,
		p_changed
	);
	if (relation_status != 0) {
		return relation_status;
	}
	int materialize_status =
		operation_solver_materialize_sequence_binder_context_view(
		ctx,
		constraint,
		ctx->typing->constraints.solutions[constraint->id].result_term,
		p_changed
	);
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
		fprintf(
			stderr,
			"sequence-binder-context-view constraint=%u classifier=%u status=%d\n",
			constraint->id,
			ctx->typing->constraints.solutions[constraint->id].result_term,
			materialize_status
		);
	}
	return materialize_status < 0 ? -1 : 0;
}

/* ContextDB is the proof layer's lexical schema. Sequence domains are solved
 * by relation equations; this projection exposes one relation answer to the
 * Context/reindex machinery without making ContextDB a competing authority. */
static int operation_solver_materialize_sequence_binder_context_view(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* equation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !equation || !p_changed || equation->kind !=
			OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t equation_id;
	int binding_status = operation_solver_register_sequence_binder_context(
		ctx, equation, &equation_id
	);
	if (binding_status != 0) {
		return binding_status < 0 ? -1 : 1;
	}
	const struct prototype_context_classifier_equation* context_equation =
		prototype_context_classifier_equation_get(
			&ctx->metadata->contexts, equation_id
		);
	if (!context_equation) {
		return -1;
	}
	if (context_equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
		const struct prototype_context_classifier_equation* source_equation =
			prototype_context_classifier_equation_get(
				&ctx->metadata->contexts, context_equation->key_value
			);
		if (!source_equation) {
			return -1;
		}
		const struct prototype_typed_projection* owner_projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections,
				equation->owner_typed_projection
			);
		uint32_t publication_projection = owner_projection ?
			operation_solver_publication_projection(
				ctx, owner_projection->source_occurrence
			) : PROTOTYPE_INVALID_ID;
		if (publication_projection == equation->owner_typed_projection &&
			source_equation->source_classifier_authority !=
				PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED) {
			/* The publication projection is the unique source-Context equation for
			 * this generated binder. It may move from a bootstrap candidate to the
			 * final recursive motive. Other projections are derived by reindexing and
			 * never write back into the source equation. */
			int publish_status = operation_solver_publish_context_classifier(
				ctx, source_equation->id, classifier,
				source_equation->answer != PROTOTYPE_INVALID_ID, p_changed
			);
			if (publish_status < 0) {
				return -1;
			}
		}
		int reindex_changed = 0;
		if (operation_solver_solve_context_classifier_reindexes(
				ctx, &reindex_changed
			) != 0) {
			return -1;
		}
		*p_changed |= reindex_changed;
		/* The relation owns the parent-Context binder answer. This derived view
		 * is consumed by typed projections. ContextDB retains its independently
		 * derived reindex view for CwF evidence and is never overwritten here. */
		return 0;
	}
	if (context_equation->source_classifier_authority ==
			PROTOTYPE_CONTEXT_CLASSIFIER_AUTHORITY_FIXED) {
		return context_equation->answer < ctx->terms->term_count &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				context_equation->answer,
				classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : 1;
	}
	return operation_solver_publish_context_classifier(
		ctx, equation_id, classifier, 1, p_changed
	);
}

static int operation_solver_materialize_sequence_binder_context_views(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->typing) {
		return -1;
	}
	for (uint32_t equation_id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		equation_id != PROTOTYPE_INVALID_ID;
		equation_id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, equation_id
		)) {
		const struct prototype_typing_constraint* equation =
			&ctx->typing->constraints.constraints[equation_id];
		if (equation->kind != OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION) {
			continue;
		}
		uint32_t classifier =
			ctx->typing->constraints.solutions[equation_id].result_term;
		if (classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int changed = 0;
		int publish_status =
			operation_solver_materialize_sequence_binder_context_view(
				ctx,
				equation,
				classifier,
			&changed
			);
		if (publish_status != 0) {
			return publish_status < 0 ? -1 : 0;
		}
	}
	return 0;
}

static int operation_solver_solve_projected_classifier(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	int* p_changed
) {
	if (!ctx || !constraint || !p_changed ||
		constraint->owner_typed_projection >=
			ctx->typing->context_projections.typed_projection_count ||
		ctx->typing->constraints.classifier_premise_count_for_projection[
			constraint->owner_typed_projection
		] == 0) {
		return -1;
	}
	uint32_t candidate = PROTOTYPE_INVALID_ID;
	const struct prototype_typing_constraint_solution* aggregate =
		&ctx->typing->constraints.solutions[constraint->id];
	if (aggregate->source_classifier_candidate != PROTOTYPE_INVALID_ID) {
		uint32_t materialized_context;
		uint32_t substitution;
		int context_status = operation_typed_projection_materialized_context(
			ctx,
			constraint->owner_typed_projection,
			&materialized_context,
			&substitution
		);
		if (context_status > 0) {
			return 0;
		}
		const struct prototype_substitution* action = context_status == 0 ?
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution
			) : NULL;
		if (context_status < 0 || !action ||
			action->source_context != materialized_context ||
			prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				aggregate->source_classifier_candidate,
				substitution,
				&candidate
			) != 0) {
			return -1;
		}
	}
	for (uint32_t premise_id = ctx->typing->constraints.
			first_classifier_premise_for_projection[
				constraint->owner_typed_projection
			];
		premise_id != PROTOTYPE_INVALID_ID;
		premise_id = ctx->typing->constraints.next_classifier_premise[premise_id]) {
		if (!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, premise_id,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			)) {
			return -1;
		}
		const struct prototype_typing_constraint_solution* premise =
			&ctx->typing->constraints.solutions[premise_id];
		int premise_kind =
			ctx->typing->constraints.constraints[premise_id].kind;
		if (premise_kind == OPERATION_CONSTRAINT_PI_DOMAIN_RELATION ||
			premise_kind == OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION) {
			continue;
		}
		if (premise->result_term == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (candidate == PROTOTYPE_INVALID_ID) {
			candidate = premise->result_term;
			continue;
		}
		if (prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				candidate,
				premise->result_term
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			continue;
		}
		{
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(stderr,
					"projected-classifier-conflict aggregate=%u projection=%u "
					"premise=%u left=%u right=%u\n",
					constraint->id, constraint->owner_typed_projection,
					premise_id, candidate, premise->result_term);
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, candidate
				);
				fputc('\n', stderr);
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, premise->result_term
				);
				fputc('\n', stderr);
			}
			struct prototype_typing_constraint_solution* aggregate =
				&ctx->typing->constraints.solutions[constraint->id];
			aggregate->contradiction_expected_classifier = candidate;
			aggregate->contradiction_actual_classifier = premise->result_term;
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					constraint->id,
					OPERATION_CONSTRAINT_STATE_CONTRADICTION,
					OPERATION_CLASSIFIER_GOAL_REASON_CONTEXT_CONTRADICTION
				) != 0) {
				return -1;
			}
			if (ctx->metadata->compile_diagnostic_count == 0) {
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
					PROTOTYPE_COMPILE_DIAGNOSTIC_CLASSIFIER_EQUATION_CONTRADICTION,
					constraint->source_ast,
					constraint->source_occurrence,
					constraint->id,
					candidate,
					premise->result_term,
					compile_source_span(ctx, constraint->source_ast)
				);
			}
			return -1;
		}
	}
	if (candidate == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	/* Effect-row metas are an input to this projection equation, not an ambient
	 * rewrite performed by every answer publication. Keeping the substitution
	 * here prevents effect finalization from replacing a projection-specific
	 * relation solution with an occurrence-wide structural seed. */
	if (operation_effect_apply_materialized_solution(
			ctx,
			constraint->owner_typed_projection,
			candidate,
			&candidate
		) != 0) {
		return -1;
	}
	/* Generated evidence classifiers carry an exact Core endpoint. DefEq is
	 * enough for type conversion, but retaining an older representative would
	 * detach Terminates/Returns-style evidence from its projected computation.
	 * Replace that representation before publishing the aggregate answer. */
	if (aggregate->source_classifier_candidate != PROTOTYPE_INVALID_ID &&
		aggregate->result_term != PROTOTYPE_INVALID_ID &&
		aggregate->result_term != candidate &&
		operation_solver_projection_answer_transition(
			ctx,
			constraint->owner_typed_projection,
			OPERATION_SOLVER_ANSWER_DISCARD,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			p_changed
		) != 0) {
		return -1;
	}
	return operation_solver_projection_answer_transition(
		ctx,
		constraint->owner_typed_projection,
		OPERATION_SOLVER_ANSWER_PUBLISH,
		candidate,
		constraint->id,
		p_changed
	);
}

static int operation_solver_solve(struct compile_context* ctx, int require_complete) {
	if (!ctx) {
		return -1;
	}
	uint32_t branch_restart_count = 0;
	int regenerate_constraints =
		!ctx->classifier_workspace.initialized ||
		ctx->classifier_workspace.initialized_occurrence_count !=
			ctx->metadata->typed_occurrences.occurrence_count;

restart_after_branch_pullback:
	if (branch_restart_count++ >
		ctx->metadata->typed_occurrences.case_count + 1) {
		fprintf(stderr, "classifier solver: branch pullback did not stabilize\n");
		return -1;
	}
	if (regenerate_constraints && operation_solver_generate_constraints(ctx) != 0) {
		fprintf(stderr, "classifier solver: constraint generation failed\n");
		return -1;
	}
	regenerate_constraints = 0;
	int changed = 0;
	if (operation_solver_seed_known_classifiers(ctx, &changed) != 0) {
		fprintf(stderr, "classifier solver: input seeding failed\n");
		return -1;
	}
	int branch_projection_waiting;
	branch_projection_waiting = 0;
	if (operation_solver_solve_branch_refinement_constraints(
			ctx, &branch_projection_waiting
		) != 0) {
		fprintf(stderr, "classifier solver: branch refinement pass failed\n");
		return -1;
	}
	for (;;) {
		uint32_t i;
		int pop_status;
		while ((pop_status = operation_solver_pop_constraint(ctx, &i)) == 0) {
			int pass_changed = 0;
			const struct prototype_typing_constraint* constraint =
				&ctx->typing->constraints.constraints[i];
			struct prototype_typing_constraint_solution* constraint_result =
				&ctx->typing->constraints.solutions[i];
			ctx->active_classifier_constraint_id = i;
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"classifier-trace constraint=%u kind=%d domain=%d target=%u state=%d reason=%d\n",
					i,
					constraint->kind,
					constraint->domain,
					constraint->source_occurrence,
					constraint_result->state,
					constraint_result->reason
				);
				fprintf(stderr, " owner-projection=%u operands=", constraint->owner_typed_projection);
				for (uint32_t operand = 0; operand < constraint->operand_count; ++operand) {
					fprintf(
						stderr,
						"%s%u:%u",
						operand == 0 ? "" : ",",
						operation_solver_constraint_operand_projection(ctx, constraint, operand),
						operation_solver_classifier_for_projection(
							ctx, operation_solver_constraint_operand_projection(ctx, constraint, operand)
						)
					);
				}
				fputc('\n', stderr);
			}
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
				if (constraint->kind > 0 && constraint->kind <
						OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT) {
					ctx->metadata->constraint_noop_by_kind[constraint->kind]++;
				}
				continue;
			}
			uint32_t target = constraint->source_occurrence;
			int effort_status = prototype_effort_consume(
				&ctx->metadata->effort,
				PROTOTYPE_EFFORT_PHASE_CLASSIFIER,
				1
			);
			if (effort_status == 1) {
				if (operation_solver_enqueue_constraint(
						ctx, i, PROTOTYPE_COMPILE_ENQUEUE_EFFORT_RESUME
					) != 0) {
					return -1;
				}
				operation_solver_refresh_constraint_states(ctx, 1);
				return 1;
			}
			if (effort_status != 0) return -1;
			uint32_t classifier;
			switch (constraint->kind) {
				case OPERATION_CONSTRAINT_EQUAL:
					if (constraint->payload.classifier.goal.reference.referenced_operation >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						constraint->operand_count != 1 ||
						(classifier = operation_solver_classifier_for_projection(
							ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
						)) ==
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if (operation_solver_publish_projection(
							ctx, constraint->owner_typed_projection, classifier,
							&pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CONVERTIBLE: {
					uint32_t expected_classifier =
						constraint->payload.classifier.goal.conversion.expected_classifier;
					constraint_result->classifier_compatibility_validated = 0;
					if (expected_classifier == PROTOTYPE_INVALID_ID) {
						expected_classifier =
							ctx->typing->seed.classifier_seeds[target].classifier;
					}
					if (constraint->payload.classifier.goal.conversion.body_occurrence >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						constraint->operand_count != 1 ||
						expected_classifier >=
							ctx->terms->term_count) {
						break;
					}
					uint32_t projected_expected;
					int expected_projection_status =
						operation_solver_term_in_projection_context(
							ctx,
							constraint->owner_typed_projection,
							expected_classifier,
							&projected_expected
						);
					if (expected_projection_status > 0) {
						break;
					}
					if (expected_projection_status < 0) {
						return -1;
					}
					expected_classifier = projected_expected;
					uint32_t conversion_context;
					uint32_t conversion_substitution;
					if (operation_typed_projection_materialized_context(
							ctx,
							constraint->owner_typed_projection,
							&conversion_context,
							&conversion_substitution
						) != 0) {
						break;
					}
					(void)conversion_substitution;
					classifier = operation_solver_classifier_for_projection(
						ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
					);
					if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
						fprintf(
							stderr,
							"convertible projection=%u operand=%u context=%u expected=%u actual=%u\n",
							constraint->owner_typed_projection,
							operation_solver_constraint_operand_projection(ctx, constraint, 0),
							conversion_context,
							expected_classifier,
							classifier
						);
					}
					if (classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					uint32_t checked_expected;
					int validation = operation_solver_validate_conversion(
						ctx,
						constraint,
						constraint_result,
						conversion_context,
						expected_classifier,
						classifier,
						&checked_expected
					);
					if (validation == OPERATION_CONVERSION_INVALID) {
						return -1;
					}
					if (validation == OPERATION_CONVERSION_EFFORT_EXHAUSTED) {
						return 1;
					}
					if (validation == OPERATION_CONVERSION_EQUAL ||
						validation == OPERATION_CONVERSION_COMPATIBLE) {
						constraint_result->classifier_compatibility_validated =
							validation == OPERATION_CONVERSION_COMPATIBLE;
						/* `::` verifies the synthesized classifier; it never replaces
						 * that classifier with the expected term. The expected term remains
						 * authoritative only inside this conversion constraint. */
						if (operation_solver_publish_projection(
								ctx,
								constraint->owner_typed_projection,
								classifier,
								&pass_changed
							) != 0) {
							return -1;
						}
						break;
					}
					if (validation == OPERATION_CONVERSION_REJECTED &&
						prototype_typing_constraint_transition(
							&ctx->typing->constraints,
							constraint->id,
							OPERATION_CONSTRAINT_STATE_CONTRADICTION,
							OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED
						) != 0) {
						return -1;
					}
					if (operation_solver_publish_projection(
							ctx,
							constraint->owner_typed_projection,
							classifier,
							&pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (constraint->payload.classifier.goal.pi.body_or_function_operation >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						(constraint->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
							constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence >=
								ctx->metadata->typed_occurrences.occurrence_count)) {
						break;
					}
					if (constraint->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
						if (operation_solver_solve_pi_intro(
								ctx, constraint, &pass_changed
							) != 0) {
							return -1;
						}
					} else {
						const struct prototype_typed_projection* traced_argument_projection =
							constraint->operand_count == 2 ?
							prototype_typed_projection_get(
								&ctx->typing->context_projections,
								operation_solver_constraint_operand_projection(ctx, constraint, 1)
							) : NULL;
						uint32_t traced_argument_operation = traced_argument_projection ?
							traced_argument_projection->source_occurrence :
							PROTOTYPE_INVALID_ID;
						if (getenv("A_PROGRAM_MOTIVE_EXPECTED_TRACE") != NULL &&
							traced_argument_operation <
								ctx->metadata->typed_occurrences.occurrence_count &&
							ctx->metadata->typed_occurrences.occurrences[
								traced_argument_operation
							].tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
							fprintf(
								stderr,
								"motive-parent-enter equation=%u parent=%u argument=%u "
								"owner-projection=%u argument-projection=%u\n",
								constraint->id,
								target,
								traced_argument_operation,
								constraint->owner_typed_projection,
								constraint->operand_count == 2 ?
									operation_solver_constraint_operand_projection(ctx, constraint, 1) :
									PROTOTYPE_INVALID_ID
							);
						}
						/* A source APP may materialize a named TypeView before all family
						 * parameters are saturated. Its classifier is obtained by replaying
						 * the declaration telescope, not by treating the intermediate
						 * TypeView classifier as a runtime-callable Pi. */
						int type_instance_status = type_instance_formation_classifier(
							ctx,
							ctx->metadata->typed_occurrences.occurrences[target].core_term,
							&classifier
						);
						if (type_instance_status < 0) {
							return -1;
						}
						if (type_instance_status == 0) {
							if (operation_solver_publish_projection(
									ctx, constraint->owner_typed_projection,
									classifier, &pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						uint32_t function_classifier = constraint->operand_count == 2 ?
							operation_solver_classifier_for_projection(
								ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
							) : PROTOTYPE_INVALID_ID;
						uint32_t argument_classifier = constraint->operand_count == 2 ?
							operation_solver_classifier_for_projection(
								ctx, operation_solver_constraint_operand_projection(ctx, constraint, 1)
							) : PROTOTYPE_INVALID_ID;
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t contextual_function_classifier;
						uint32_t contextual_argument_classifier;
						uint32_t contextual_argument_term;
						uint32_t argument_operation = constraint->payload.classifier.
							goal.pi.domain_classifier_or_argument_occurrence;
						int application_context_status =
							operation_solver_application_inputs_in_projection_context(
								ctx,
								constraint->owner_typed_projection,
								function_classifier,
								argument_classifier,
								ctx->metadata->typed_occurrences.occurrences[
									argument_operation
								].core_term,
								&contextual_function_classifier,
								&contextual_argument_classifier,
								&contextual_argument_term
							);
						if (application_context_status < 0) {
							return -1;
						}
						if (application_context_status > 0) {
							break;
							}
							function_classifier = contextual_function_classifier;
							argument_classifier = contextual_argument_classifier;
							int static_family =
								ctx->metadata->typed_occurrences.occurrences[target].
									application_role ==
									PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION;
							uint32_t expected_argument_domain = PROTOTYPE_INVALID_ID;
							uint32_t specialized_function = PROTOTYPE_INVALID_ID;
							uint32_t function_pi = PROTOTYPE_INVALID_ID;
							int argument_pi_status = static_family ? 0 :
								operation_callable_classifier_parts(
									ctx,
									function_classifier,
									argument_classifier,
									contextual_argument_term,
									0,
									&specialized_function,
									&function_pi,
									&expected_argument_domain
								);
						if (getenv("A_PROGRAM_MOTIVE_EXPECTED_TRACE") != NULL &&
							traced_argument_operation <
								ctx->metadata->typed_occurrences.occurrence_count &&
							ctx->metadata->typed_occurrences.occurrences[
								traced_argument_operation
							].tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
							fprintf(
								stderr,
								"motive-parent-domain parent=%u argument=%u projection=%u "
								"function=%u argument-classifier=%u status=%d expected=%u\n",
								target,
								traced_argument_operation,
								operation_solver_constraint_operand_projection(ctx, constraint, 1),
								function_classifier,
								argument_classifier,
								argument_pi_status,
								argument_pi_status == 0 ? expected_argument_domain :
									PROTOTYPE_INVALID_ID
							);
						}
						if (argument_pi_status < 0) {
							return -1;
						}
							if (argument_pi_status == 0 && !static_family) {
							if (argument_classifier != PROTOTYPE_INVALID_ID) {
								struct prototype_term_conversion_result domain_conversion =
									prototype_judgement_classifier_conversion(
										ctx->terms,
										ctx->type_declarations,
										expected_argument_domain,
										argument_classifier
									);
								if (domain_conversion.status ==
										PROTOTYPE_TERM_CONVERSION_NOT_EQUAL &&
									!prototype_judgement_classifier_compatible(
										ctx->terms,
										ctx->type_declarations,
										expected_argument_domain,
										argument_classifier
									)) {
									int refinement_changed = 0;
									int refine_status =
										operation_refine_projected_classifier_from_expected(
											ctx,
											operation_solver_constraint_operand_projection(
												ctx, constraint, 1
											),
											expected_argument_domain,
											constraint->id,
											0,
											&refinement_changed
										);
									if (refine_status < 0) {
										return -1;
									}
									if (refinement_changed) {
										pass_changed = 1;
										break;
									}
									uint32_t instantiated_argument;
									int instantiate_status =
										prototype_judgement_solve_expected_effect_rows(
											ctx->terms,
											ctx->type_declarations,
											NULL,
											expected_argument_domain,
											argument_classifier,
											&instantiated_argument
										);
									if (instantiate_status < 0) {
										return -1;
									}
									if (instantiate_status == 0) {
										/* Effect metavariables are solved on this APP edge. The
										 * argument projection retains its structural classifier;
										 * elimination consumes the locally instantiated view. */
										argument_classifier = instantiated_argument;
									} else {
									if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
										fprintf(stderr,
											"application-domain-contradiction constraint=%u "
											"projection=%u expected=%u actual=%u\n",
											constraint->id,
											constraint->owner_typed_projection,
											expected_argument_domain,
											argument_classifier);
										prototype_type_term_print_debug(
											stderr, ctx->symbols,
											ctx->intrinsic_environment,
											ctx->type_declarations, ctx->terms,
											expected_argument_domain
										);
										fputc('\n', stderr);
										prototype_type_term_print_debug(
											stderr, ctx->symbols,
											ctx->intrinsic_environment,
											ctx->type_declarations, ctx->terms,
											argument_classifier
										);
										fputc('\n', stderr);
									}
									constraint_result->contradiction_expected_classifier =
										expected_argument_domain;
									constraint_result->contradiction_actual_classifier =
										argument_classifier;
									if (prototype_typing_constraint_transition(
											&ctx->typing->constraints,
											constraint->id,
											OPERATION_CONSTRAINT_STATE_CONTRADICTION,
											OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_DOMAIN_CONTRADICTION
										) != 0) {
										return -1;
									}
									break;
									}
								}
							}
						}
							(void)specialized_function;
							(void)function_pi;
							uint32_t application_context;
							uint32_t application_substitution;
							int context_status =
								operation_typed_projection_materialized_context(
									ctx,
									constraint->owner_typed_projection,
									&application_context,
									&application_substitution
								);
							if (context_status < 0) {
								return -1;
							}
							if (context_status > 0) {
								break;
							}
							(void)application_substitution;
							int apply_status;
							if (static_family) {
								apply_status = operation_apply_pure_type_family_classifier(
									ctx,
									application_context,
									function_classifier,
									argument_classifier,
								contextual_argument_term,
									&classifier
								);
							} else {
								apply_status =
									prototype_judgement_app_elim_classifier_in_context(
										&ctx->metadata->contexts,
										&ctx->metadata->substitutions,
									ctx->terms,
									ctx->type_declarations,
									application_context,
									function_classifier,
									contextual_argument_term,
										argument_classifier,
										&classifier
									);
							}
						if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL &&
							apply_status != 0) {
							fprintf(
								stderr,
								"application-classifier projection=%u status=%d "
								"function=%u argument=%u\n",
								constraint->owner_typed_projection,
								apply_status,
								function_classifier,
								argument_classifier
							);
							prototype_type_term_print_debug(
								stderr, ctx->symbols, ctx->intrinsic_environment,
								ctx->type_declarations, ctx->terms, function_classifier
							);
							fputc('\n', stderr);
						}
						if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL &&
							apply_status == 0) {
							fprintf(
								stderr,
								"application-classifier-result constraint=%u projection=%u "
								"classifier=%u\n",
								i,
								constraint->owner_typed_projection,
								classifier
							);
						}
						/* TypedProjection fixes the source occurrence and Context. A rigid
						 * domain mismatch was recorded above on this exact equation; only a
						 * genuinely incomplete callable shape remains pending here. */
						if (apply_status == 0 && operation_solver_publish_projection(
								ctx, constraint->owner_typed_projection, classifier,
								&pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_PI_DOMAIN_RELATION:
					if (operation_solver_solve_pi_domain_relation(
							ctx, constraint, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION:
					if (operation_solver_solve_sequence_binder_relation(
							ctx, constraint, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_PROJECTED_CLASSIFIER:
					if (operation_solver_solve_projected_classifier(
							ctx, constraint, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION: {
					int propagation_status =
						operation_solver_propagate_constructor_domains(
							ctx, target, i, &pass_changed
						);
					if (propagation_status != 0) {
						fprintf(stderr,
							"constructor domain propagation failed constraint=%u operation=%u status=%d\n",
							i, target, propagation_status);
						return -1;
					}
					if (pass_changed) {
						if (operation_solver_enqueue_constraint(
								ctx, i, PROTOTYPE_COMPILE_ENQUEUE_RETRY
							) != 0) {
							return -1;
						}
						break;
					}
					struct operation_solver_constructor_mismatch mismatch;
					int constructor_status =
						operation_solver_constructor_spine_classifier(
							ctx, target, i, &classifier, &pass_changed, &mismatch
						);
					if (constructor_status < 0) {
						fprintf(stderr,
							"constructor classifier failed constraint=%u operation=%u status=%d tag=%d role=%d context=%u\n",
							i, target, constructor_status,
							ctx->metadata->typed_occurrences.occurrences[target].tag,
							ctx->metadata->typed_occurrences.occurrences[target].application_role,
							ctx->metadata->typed_occurrences.occurrences[target].context_id);
						return -1;
					}
					if (constructor_status ==
							OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING &&
						pass_changed) {
						if (operation_solver_enqueue_constraint(
								ctx, i, PROTOTYPE_COMPILE_ENQUEUE_RETRY
							) != 0) {
							return -1;
						}
						changed = 1;
						break;
					}
					if (constructor_status ==
							OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION) {
						if (pass_changed) {
							changed = 1;
							break;
						}
						int waits_for_branch =
							operation_solver_occurrence_waits_for_branch_action(
								ctx, target
							);
						if (waits_for_branch < 0) {
							return -1;
						}
						if (waits_for_branch > 0) {
							break;
						}
						const struct prototype_typed_occurrence* operation =
								&ctx->metadata->typed_occurrences.occurrences[target];
						(void)add_constructor_compile_diagnostic(
							ctx,
							operation->source_ast,
							target,
							constraint->id,
							mismatch.expected_classifier,
							mismatch.actual_classifier,
							mismatch.context_id,
							mismatch.constructor_ordinal,
							mismatch.field_ordinal,
							compile_source_span(ctx, operation->source_ast)
						);
						return -1;
					}
					/* The declaration telescope is the constructor classifier authority.
					 * A prior pass may have projected an under-refined indexed result;
					 * replace that projection after the dependent fields are solved. */
					if (constructor_status == 0 && operation_solver_publish_projection(
							ctx, constraint->owner_typed_projection, classifier,
							&pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_BINDER_TYPE: {
					int binding_status =
						operation_solver_projected_binding_classifier(
							ctx, constraint, &classifier
						);
					if (binding_status < 0) {
						return -1;
					}
					if (binding_status == 0 &&
						operation_solver_publish_projection(
							ctx, constraint->owner_typed_projection,
							classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_HAS_TYPE:
					if (operation_solver_classifier_for_projection(
							ctx, constraint->owner_typed_projection
						) !=
						PROTOTYPE_INVALID_ID) {
						break;
					}
					/* Core facts may seed atoms only, never a typed operation edge. */
					if (ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_APP ||
						ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
						ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
						break;
					}
						{
							uint32_t classifiers[32];
							uint32_t classifier_count = 0;
							const struct prototype_typed_occurrence* target_operation =
								&ctx->metadata->typed_occurrences.occurrences[target];
							if (target_operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR &&
								target_operation->declared_classifier != PROTOTYPE_INVALID_ID) {
								classifiers[classifier_count++] =
									target_operation->declared_classifier;
							} else if (operation_solver_collect_input_classifiers(
									ctx,
									target,
								classifiers,
								32,
								&classifier_count
								) != 0) {
								return -1;
							}
							if (getenv("A_PROGRAM_HAS_TYPE_TRACE") != NULL &&
								ctx->metadata->typed_occurrences.occurrences[target].tag ==
									PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR &&
								classifier_count != 1) {
								fprintf(
									stderr,
									"constructor-has-type projection=%u occurrence=%u seed=%u "
									"facts=%u",
									constraint->owner_typed_projection, target,
									ctx->typing->seed.classifier_seeds[target].classifier,
									classifier_count
								);
								for (uint32_t classifier_index = 0;
									classifier_index < classifier_count;
									++classifier_index) {
									fprintf(stderr, " %u", classifiers[classifier_index]);
								}
								fputc('\n', stderr);
							}
							if (classifier_count == 1) {
								uint32_t projected_classifier;
								int projection_status =
								operation_solver_term_in_projection_context(
									ctx,
									constraint->owner_typed_projection,
									classifiers[0],
										&projected_classifier
									);
								if (projection_status < 0) {
								return -1;
							}
							if (projection_status == 0 && operation_solver_publish_projection(
									ctx, constraint->owner_typed_projection,
									projected_classifier, &pass_changed
								) != 0) {
								return -1;
							}
						}
					}
					break;
				case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
					if (operation_solver_solve_match(
							ctx, target,
							constraint->owner_typed_projection,
							&pass_changed
						) != 0) {
						const struct prototype_typed_occurrence* failed =
							&ctx->metadata->typed_occurrences.occurrences[target];
						fprintf(
							stderr,
							"classifier solver: Match failed operation=%u ast=%u term=%u classifier=%u cases=%u\n",
							target,
							failed->source_ast,
							failed->core_term,
							operation_solver_classifier(ctx, target),
							failed->case_count
						);
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_IH_EXPECTED:
					if (operation_solver_materialize_induction_hypothesis(
							ctx, target, constraint->owner_typed_projection,
							&pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
					if (constraint->payload.classifier.goal.cbpv_boundary.child_operation >=
						ctx->metadata->typed_occurrences.occurrence_count) {
						return -1;
					}
					uint32_t child_classifier =
						constraint->operand_count == 1 ?
						operation_solver_classifier_for_projection(
							ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
						) : PROTOTYPE_INVALID_ID;
					if (child_classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					int boundary_status = prototype_judgement_cbpv_boundary_classifier(
							ctx->terms,
							ctx->type_declarations,
							ctx->metadata->typed_occurrences.occurrences[target].core_term,
							child_classifier,
							&classifier
						);
					/* A child may still carry its synthesis frontier while a later
					 * constructor-domain or APP constraint supplies the value/computation
					 * boundary. Keep this equation pending instead of turning worklist
					 * order into a typing error. The strict kernel helper is rerun once
					 * that occurrence classifier changes. */
					if (boundary_status != 0) {
						break;
					}
					if (operation_solver_publish_projection(
							ctx, constraint->owner_typed_projection, classifier,
							&pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT: {
					const struct prototype_typed_occurrence* fold =
						&ctx->metadata->typed_occurrences.occurrences[target];
					if (fold->core_term >= ctx->terms->term_count ||
						ctx->terms->terms[fold->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
						return -1;
					}
					int status = ctx->terms->terms[fold->core_term].as.computation_fold.clause_count == 0 ?
						operation_solver_propagate_zero_clause_computation_fold_input(
							ctx, constraint, &pass_changed
						) : operation_solver_propagate_clause_computation_fold_input(
							ctx, constraint, &pass_changed
					);
					if (status != 0) {
						fprintf(stderr,
							"computation fold input propagation failed operation=%u status=%d "
							"clauses=%u\n",
							target, status,
							ctx->terms->terms[fold->core_term].as.computation_fold.
								clause_count);
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
					/* These occurrence constraints are solved by the CBPV computation
					 * constraint pass. Their state is published only after that pass has
					 * either produced closed evidence or a VerificationDB obligation. */
					break;
				case OPERATION_CONSTRAINT_TERMINATION_WITNESS: {
					if (constraint->operand_count != 1) {
						return -1;
					}
					uint32_t computation_context;
					uint32_t computation_substitution;
					uint32_t computation_subject;
					uint32_t computation_classifier;
					int computation_status =
						operation_typed_projection_materialized_typing_answer(
							ctx,
							operation_solver_constraint_operand_projection(ctx, constraint, 0),
							&computation_context,
							&computation_substitution,
							&computation_subject,
							&computation_classifier
						);
					if (computation_status > 0) {
						break;
					}
					if (computation_status < 0 ||
						prototype_term_terminates_type(
							ctx->terms, computation_subject, &classifier
						) != 0 || operation_solver_publish_projection(
							ctx,
							constraint->owner_typed_projection,
							classifier,
							&pass_changed
						) != 0) {
						return -1;
					}
					(void)computation_context;
					(void)computation_substitution;
					(void)computation_classifier;
					break;
				}
				default:
					return -1;
			}
			if (pass_changed) {
				changed = 1;
				if (constraint->kind > 0 && constraint->kind <
						OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT) {
					ctx->metadata->constraint_changed_by_kind[constraint->kind]++;
				}
			} else if (constraint->kind > 0 && constraint->kind <
					OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT) {
				ctx->metadata->constraint_noop_by_kind[constraint->kind]++;
			}
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				const struct prototype_typing_constraint_solution* trace_answer =
					&ctx->typing->constraints.solutions[
						ctx->typing->constraints.classifier_constraint_for_projection[
							constraint->owner_typed_projection
						]
					];
				fprintf(
					stderr,
					"classifier-trace-result constraint=%u changed=%d "
					"classifier=%u answer-state=%d answer-revision=%" PRIu64
					" binder=%u revision=%" PRIu64 "\n",
					i,
					pass_changed,
					operation_solver_classifier(ctx, target),
					trace_answer->answer_state,
					trace_answer->answer_revision,
					operation_solver_binder_classifier(ctx, target),
					ctx->typing->constraints.semantic_transition_count
				);
			}
			ctx->active_classifier_constraint_id = PROTOTYPE_INVALID_ID;
		}
		if (pop_status < 0) {
			return -1;
		}
		branch_projection_waiting = 0;
		if (operation_solver_solve_branch_refinement_constraints(
				ctx, &branch_projection_waiting
			) != 0) {
			fprintf(stderr, "classifier solver: branch refinement pass failed\n");
			return -1;
		}
		int materialized = 0;
		if (operation_solver_materialize_solved_motives(
				ctx, &materialized
			) != 0) {
			fprintf(stderr, "classifier solver: motive materialization pass failed\n");
			return -1;
		}
		if (branch_projection_waiting && !materialized) {
			return 0;
		}
		if (!materialized) {
			if (operation_solver_occurrence_topology_grew(ctx)) {
				regenerate_constraints = 1;
				goto restart_after_branch_pullback;
			}
			if (require_complete && operation_solver_require_complete(ctx) != 0) {
				operation_solver_refresh_constraint_states(ctx, 1);
				return -1;
			}
			operation_solver_refresh_constraint_states(ctx, 0);
			return 0;
		}
		changed = 1;
	}
}
