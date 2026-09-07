/* Motive synthesis is typed compiler work above the context-free Core graph.
 * A branch index correspondence is derived from the indexed family telescope
 * and the solved branch substitution. Core shape is never used to discover a
 * replacement location. */

enum operation_motive_transition_reason {
	OPERATION_MOTIVE_TRANSITION_DEPENDENCY = 1,
	OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION,
	OPERATION_MOTIVE_TRANSITION_RECURSIVE_EQUATION,
	OPERATION_MOTIVE_TRANSITION_EXPECTED_EQUATION,
	OPERATION_MOTIVE_TRANSITION_MATERIALIZATION,
	OPERATION_MOTIVE_TRANSITION_CONTRADICTION,
	OPERATION_MOTIVE_TRANSITION_RESIDUAL,
	OPERATION_MOTIVE_TRANSITION_REBUILD,
	OPERATION_MOTIVE_TRANSITION_RESTORE,
	OPERATION_MOTIVE_TRANSITION_PROJECTION
};

static int operation_motive_has_recursive_equations(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection
);

static int operation_solver_abstract_indexed_motive(
	struct compile_context* ctx,
	uint32_t value,
	uint32_t value_classifier,
	uint32_t result_classifier,
	uint32_t* p_motive
);

static int operation_term_graph_contains_exact(
	const struct prototype_term_db* terms,
	uint32_t root,
	uint32_t needle,
	int* p_found
) {
	if (!terms || !p_found || root >= terms->term_count ||
		needle >= terms->term_count) {
		return -1;
	}
	*p_found = 0;
	unsigned char* visited = calloc(terms->term_count, sizeof(*visited));
	uint32_t* pending = malloc(terms->term_count * sizeof(*pending));
	if (!visited || !pending) {
		free(visited);
		free(pending);
		return -1;
	}
	size_t pending_count = 1;
	pending[0] = root;
	visited[root] = 1;
	while (pending_count > 0) {
		uint32_t current = pending[--pending_count];
		if (current == needle) {
			*p_found = 1;
			break;
		}
		uint32_t child_count;
		if (prototype_term_child_count(terms, current, &child_count) != 0) {
			free(visited);
			free(pending);
			return -1;
		}
		for (uint32_t i = 0; i < child_count; ++i) {
			struct prototype_term_child child;
			if (prototype_term_child_at(terms, current, i, &child) != 0 ||
				child.term >= terms->term_count) {
				free(visited);
				free(pending);
				return -1;
			}
			if (!visited[child.term]) {
				visited[child.term] = 1;
				pending[pending_count++] = child.term;
			}
		}
	}
	free(visited);
	free(pending);
	return 0;
}

static int operation_expected_classifier_depends_on_family_index(
	const struct compile_context* ctx,
	uint32_t value_classifier,
	uint32_t expected_classifier,
	int* p_depends
) {
	if (!ctx || !ctx->terms || !ctx->type_declarations || !p_depends ||
		value_classifier >= ctx->terms->term_count ||
		expected_classifier >= ctx->terms->term_count) {
		return -1;
	}
	*p_depends = 0;
	uint32_t type_id;
	uint32_t arguments[32];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			value_classifier,
			&type_id,
			arguments,
			&argument_count
		) != 0 || type_id >=
			ctx->type_declarations->semantic_schema.type_count) {
		return 0;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (declaration->index_count > 32 ||
		declaration->parameter_count + declaration->index_count !=
			argument_count) {
		return -1;
	}
	for (uint32_t i = 0; i < declaration->index_count; ++i) {
		int found;
		if (operation_term_graph_contains_exact(
				ctx->terms,
				expected_classifier,
				arguments[declaration->parameter_count + i],
				&found
			) != 0) {
			return -1;
		}
		if (found) {
			*p_depends = 1;
			return 0;
		}
	}
	return 0;
}

static int operation_match_owns_induction_hypothesis(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	int* p_owns
) {
	if (!ctx || !ctx->metadata || !p_owns || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[match_operation_id].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return -1;
	}
	*p_owns = 0;
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (occurrence->tag ==
				PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
			occurrence->ih_owner_occurrence == match_operation_id) {
			*p_owns = 1;
			break;
		}
	}
	return 0;
}

/* Publish the complete Layer T motive cell through one authority. Callers may
 * construct Core lambda/application Terms, but only this transition records
 * their meaning as a Match motive. */
static int operation_motive_solution_transition_for_projection(
	struct compile_context* ctx,
	uint32_t match_typed_projection,
	const struct prototype_typing_motive_solution* next,
	enum operation_motive_transition_reason reason,
	int wake_dependents,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !ctx->core_requests || !next ||
		reason < OPERATION_MOTIVE_TRANSITION_DEPENDENCY ||
		reason > OPERATION_MOTIVE_TRANSITION_PROJECTION ||
		ctx->typing->constraints.topology_sealed || match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count ||
		next->status > OPERATION_MOTIVE_STATUS_CONTRADICTION ||
		next->phase > OPERATION_MOTIVE_PHASE_MATERIALIZED) {
		return -1;
	}
	struct prototype_core_inspect_counts core_counts;
	if (typing_core_inspect_counts(
			ctx->core_requests, &core_counts, NULL
		) != 0) {
		return -1;
	}
	if ((next->status == OPERATION_MOTIVE_STATUS_SOLVED) !=
		(next->phase == OPERATION_MOTIVE_PHASE_MATERIALIZED &&
		 next->motive < core_counts.term_count)) {
		return -1;
	}
	int has_expected = next->expected_equation_classifier !=
		PROTOTYPE_INVALID_ID;
	if (has_expected != (next->expected_equation_constraint_id !=
			PROTOTYPE_INVALID_ID) || has_expected !=
		(next->expected_equation_context_id != PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, match_typed_projection
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		].tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return -1;
	}
	struct prototype_typing_motive_solution* current =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (memcmp(current, next, sizeof(*current)) == 0) {
		return 0;
	}
	*current = *next;
	if (p_changed) {
		*p_changed = 1;
	}
	return wake_dependents ? operation_solver_enqueue_projection_dependents(
		ctx, match_typed_projection
	) : 0;
}

static int operation_motive_reopen_after_branch_refinement(
	struct compile_context* ctx,
	uint32_t match_typed_projection
) {
	if (!ctx || match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (next.phase != OPERATION_MOTIVE_PHASE_MATERIALIZED) {
		return 0;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, match_typed_projection
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	int recursive = operation_motive_has_recursive_equations(
		ctx, projection->source_occurrence, match_typed_projection
	);
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || recursive < 0) {
		return -1;
	}
	/* The old motive was validated against a weaker branch action and is not a
	 * sound fixed-point approximation in the refined Context. Drop only that
	 * answer; the immutable equations and their SCC remain available for the
	 * ordinary dependency scheduler to recompute it. */
	next.motive = PROTOTYPE_INVALID_ID;
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	next.phase = recursive ? OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE :
		OPERATION_MOTIVE_PHASE_EQUATION_SET;
	return operation_motive_solution_transition_for_projection(
		ctx,
		match_typed_projection,
		&next,
		OPERATION_MOTIVE_TRANSITION_REBUILD,
		0,
		NULL
	);
}

static int operation_motive_mark_equations_ready(
	struct compile_context* ctx,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (next.phase == OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE) {
		/* A guarded recursive equation is stronger than the observation that
		 * every branch currently has a classifier. Those classifiers include
		 * provisional IH approximations and must not demote the recursive
		 * solution back to an ordinary finite equation set. */
		return 0;
	}
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	next.phase = OPERATION_MOTIVE_PHASE_EQUATION_SET;
	return operation_motive_solution_transition_for_projection(
		ctx, match_typed_projection, &next,
		OPERATION_MOTIVE_TRANSITION_DEPENDENCY, 0, NULL
	);
}

static int operation_motive_mark_waiting(
	struct compile_context* ctx,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (next.status == OPERATION_MOTIVE_STATUS_SOLVED ||
		next.status == OPERATION_MOTIVE_STATUS_CONTRADICTION) {
		return 0;
	}
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	return operation_motive_solution_transition_for_projection(
		ctx, match_typed_projection, &next,
		OPERATION_MOTIVE_TRANSITION_DEPENDENCY, 0, NULL
	);
}

static int operation_motive_mark_guarded_recursive(
	struct compile_context* ctx,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	next.phase = OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE;
	return operation_motive_solution_transition_for_projection(
		ctx, match_typed_projection, &next,
		OPERATION_MOTIVE_TRANSITION_RECURSIVE_EQUATION, 0, NULL
	);
}

static int operation_motive_offer_expected_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t* p_motive_application,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !ctx->terms || !p_motive_application ||
		!p_changed ||
		ctx->typing->constraints.topology_sealed ||
		match_operation_id >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		expected_classifier >= ctx->terms->term_count ||
		!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, origin_constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return -1;
	}
	*p_motive_application = PROTOTYPE_INVALID_ID;
	const struct prototype_typing_constraint* origin =
		&ctx->typing->constraints.constraints[origin_constraint_id];
	if (origin->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
		return -1;
	}
	uint32_t match_typed_projection = PROTOTYPE_INVALID_ID;
	if (origin->source_occurrence == match_operation_id) {
		match_typed_projection = origin->owner_typed_projection;
	} else {
		int find_status = operation_solver_find_projection_in_subtree(
			ctx,
			origin->owner_typed_projection,
			match_operation_id,
			0,
			&match_typed_projection
		);
		if (find_status < 0) {
			return -1;
		}
		if (find_status > 0 || match_typed_projection == PROTOTYPE_INVALID_ID) {
			return 0;
		}
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return -1;
	}
	struct prototype_typing_motive_solution* solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	uint32_t materialized_context;
	uint32_t materialized_substitution;
	int context_status = operation_typed_projection_materialized_context(
		ctx,
		match_typed_projection,
		&materialized_context,
		&materialized_substitution
	);
	if (context_status != 0) {
		if (context_status < 0) {
			return -1;
		}
		/* The expected result is an equation even while its target Context is
		 * pending. Persist it in the source Match Context so the Match projection
		 * can consume and reindex it when its Context projection is solved. */
		if (solution->expected_equation_classifier == PROTOTYPE_INVALID_ID) {
			struct prototype_typing_motive_solution next = *solution;
			next.expected_equation_classifier = expected_classifier;
			next.expected_equation_constraint_id = origin->id;
			next.expected_equation_context_id = match_operation->context_id;
			if (next.status != OPERATION_MOTIVE_STATUS_SOLVED) {
				next.status = OPERATION_MOTIVE_STATUS_WAITING;
			}
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &next,
					OPERATION_MOTIVE_TRANSITION_EXPECTED_EQUATION,
					0, p_changed
				) != 0) {
				return -1;
			}
		} else if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				solution->expected_equation_classifier,
				expected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			struct prototype_typing_motive_solution contradiction = *solution;
			contradiction.status = OPERATION_MOTIVE_STATUS_CONTRADICTION;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &contradiction,
					OPERATION_MOTIVE_TRANSITION_CONTRADICTION,
					1, p_changed
				) != 0) {
				return -1;
			}
			return -1;
		}
		return 0;
	}
	uint32_t source_expected = expected_classifier;
	if (solution->expected_equation_classifier < ctx->terms->term_count) {
		source_expected = solution->expected_equation_classifier;
	}
	uint32_t projected_expected;
	if (solution->expected_equation_context_id == materialized_context &&
		solution->expected_equation_classifier < ctx->terms->term_count) {
		projected_expected = source_expected;
	} else {
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				source_expected,
				materialized_substitution,
				&projected_expected
			) != 0) {
			return -1;
		}
	}
	expected_classifier = projected_expected;
	if (getenv("A_PROGRAM_MOTIVE_EXPECTED_TRACE")) {
		fprintf(stderr,
			"motive-expected operation=%u projection=%u origin=%u "
			"expected=%u context-status=%d context=%u substitution=%u\n",
			match_operation_id,
			match_typed_projection,
			origin_constraint_id,
			expected_classifier,
			context_status,
			materialized_context,
			materialized_substitution);
		fprintf(stderr, "motive-expected-term ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
			expected_classifier
		);
		fprintf(stderr, "\n");
	}
	uint32_t scrutinee_occurrence;
	if (compile_occurrence_child(
			ctx, match_operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_occurrence
		) != 0 || match_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_operation->core_term].tag !=
			PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t scrutinee = ctx->metadata->typed_occurrences.occurrences[
		scrutinee_occurrence
	].core_term;
	if (scrutinee >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t scrutinee_typed_projection;
	if (operation_solver_projected_child(
			ctx,
			match_typed_projection,
			PROTOTYPE_TERM_CHILD_SCRUTINEE,
			0,
			&scrutinee_typed_projection
		) != 0) {
		return -1;
	}
	uint32_t scrutinee_classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_typed_projection
	);
	if (solution->expected_equation_classifier < ctx->terms->term_count &&
		solution->expected_equation_context_id == materialized_context &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations,
			solution->expected_equation_classifier, expected_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* Two immutable typing premises demand incompatible results from the
		 * same Match occurrence. Do not let processing order choose a motive. */
		struct prototype_typing_motive_solution contradiction = *solution;
		contradiction.status = OPERATION_MOTIVE_STATUS_CONTRADICTION;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &contradiction,
				OPERATION_MOTIVE_TRANSITION_CONTRADICTION, 1, p_changed
			) != 0) {
			return -1;
		}
		return -1;
	}
	if (solution->expected_equation_classifier == PROTOTYPE_INVALID_ID ||
		solution->expected_equation_context_id != materialized_context) {
		struct prototype_typing_motive_solution next = *solution;
		next.expected_equation_classifier = expected_classifier;
		next.expected_equation_constraint_id = origin->id;
		next.expected_equation_context_id = materialized_context;
		if (next.status != OPERATION_MOTIVE_STATUS_SOLVED) {
			next.status = OPERATION_MOTIVE_STATUS_WAITING;
		}
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_EXPECTED_EQUATION, 0, p_changed
			) != 0) {
			return -1;
		}
		solution = &ctx->typing->constraints.
			motive_solution_for_projection[match_typed_projection];
	}
	if (scrutinee_classifier >= ctx->terms->term_count) {
		return 0;
	}

	uint32_t motive = solution->motive;
	int reuse_motive = motive < ctx->terms->term_count &&
		prototype_judgement_apply_indexed_motive(
			ctx->terms, ctx->type_declarations, motive, scrutinee,
			scrutinee_classifier, 0, p_motive_application
		) == 0 && prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations,
			*p_motive_application, expected_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	if (!reuse_motive) {
		int owns_induction_hypothesis;
		if (operation_match_owns_induction_hypothesis(
				ctx, match_operation_id, &owns_induction_hypothesis
			) != 0) {
			return -1;
		}
		int depends_on_index;
		if (operation_expected_classifier_depends_on_family_index(
				ctx,
				scrutinee_classifier,
				expected_classifier,
				&depends_on_index
			) != 0) {
			return -1;
		}
		int requires_indexed_motive =
			depends_on_index && owns_induction_hypothesis;
		if (requires_indexed_motive) {
			int indexed_status = operation_solver_abstract_indexed_motive(
				ctx, scrutinee, scrutinee_classifier, expected_classifier,
				&motive
			);
			if (indexed_status != 0) {
				return indexed_status < 0 ? -1 : 0;
			}
		} else {
			uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
			uint32_t motive_var;
			uint32_t motive_body;
			int abstraction_status;
			if (motive_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, motive_binder, &motive_var
				) != 0) {
				return -1;
			}
			if (ctx->terms->terms[scrutinee].tag == PROTOTYPE_TERM_VAR) {
				abstraction_status = prototype_term_graph_substitute_bound_var(
					ctx->terms, expected_classifier,
					ctx->terms->terms[scrutinee].as.var.binding_id,
					motive_var, &motive_body
				);
			} else {
				abstraction_status = prototype_term_graph_replace_exact(
					ctx->terms, expected_classifier, scrutinee,
					motive_var, &motive_body
				);
			}
			if (abstraction_status != 0 || prototype_term_lambda(
					ctx->terms, motive_binder, motive_body, &motive
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_apply_indexed_motive(
				ctx->terms, ctx->type_declarations, motive, scrutinee,
				scrutinee_classifier, 0, p_motive_application
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				*p_motive_application, expected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		int recursive_status = operation_motive_has_recursive_equations(
			ctx, match_operation_id, match_typed_projection
		);
		if (recursive_status < 0) {
			return -1;
		}
		if (recursive_status > 0 && solution->motive < ctx->terms->term_count &&
			solution->motive != motive && !requires_indexed_motive) {
			/* The guarded recursive equation is stronger than this projected
			 * expectation. Its eventual validation decides whether the expected
			 * equation is satisfied; the expectation cannot replace it. */
			*p_motive_application = PROTOTYPE_INVALID_ID;
			return 0;
		}
	}
	if (solution->motive != motive ||
		solution->phase != OPERATION_MOTIVE_PHASE_MATERIALIZED) {
		struct prototype_typing_motive_solution next = *solution;
		next.motive = motive;
		next.constant_candidate = PROTOTYPE_INVALID_ID;
		next.status = OPERATION_MOTIVE_STATUS_SOLVED;
		next.phase = OPERATION_MOTIVE_PHASE_MATERIALIZED;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_MATERIALIZATION, 1, p_changed
			) != 0) {
			return -1;
		}
	}
	return 1;
}

static const struct prototype_typing_constraint*
operation_solver_motive_equation_for_projection(
	const struct compile_context* ctx,
	uint32_t match_typed_projection,
	uint32_t case_index
) {
	if (!ctx || !ctx->metadata || match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return NULL;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, match_typed_projection
		);
	if (!projection || projection->source_occurrence >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return NULL;
	}
	uint32_t match_operation = projection->source_occurrence;
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[match_operation];
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		case_index >= match->case_count || match->first_case + case_index >=
		ctx->metadata->typed_occurrences.case_count) {
		return NULL;
	}
	uint32_t match_case_projection;
	if (match_typed_projection == PROTOTYPE_INVALID_ID ||
		prototype_typed_match_case_projection_find(
			&ctx->typing->context_projections,
			match->first_case + case_index,
			match_typed_projection,
			&match_case_projection
		) != 0) {
		return NULL;
	}
	uint32_t constraint_id = ctx->typing->constraints.
		motive_constraint_for_match_case_projection[match_case_projection];
	if (!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return NULL;
	}
	const struct prototype_typing_constraint* constraint =
		&ctx->typing->constraints.constraints[constraint_id];
	return constraint->kind == OPERATION_CONSTRAINT_MOTIVE_EQUATION &&
		constraint->source_occurrence == match_operation &&
		constraint->payload.classifier.goal.motive_case.case_index == case_index ?
			constraint : NULL;
}

/* A branch classifier is synthesized in the source branch Context. Dependent
 * motive construction must first pull that answer into the refined branch
 * Context selected by the matching branch-refinement equation. The refinement
 * solution is the authority; the published typed-occurrence case is only an
 * accepted projection of that answer. */
static int operation_motive_case_refinement_substitution(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* motive_equation,
	uint32_t refined_branch_context,
	uint32_t* p_substitution
) {
	if (!ctx || !motive_equation || !p_substitution ||
		motive_equation->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION ||
		motive_equation->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
		return -1;
	}
	uint32_t case_projection = motive_equation->payload.classifier.goal.
		motive_case.match_case_projection;
	if (case_projection >=
			ctx->typing->context_projections.match_case_projection_count) {
		return -1;
	}
	uint32_t refinement_id = ctx->typing->constraints.
		branch_refinement_constraint_for_match_case_projection[case_projection];
	if (refinement_id >= ctx->typing->constraints.constraint_count) {
		return 1;
	}
	const struct prototype_typing_constraint* refinement =
		&ctx->typing->constraints.constraints[refinement_id];
	const struct prototype_typing_constraint_solution* solution =
		&ctx->typing->constraints.solutions[refinement_id];
	if (refinement->domain !=
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
		refinement->payload.branch_refinement.match_case_projection !=
			case_projection) {
		return -1;
	}
	if (solution->branch_refinement.refinement_status ==
			PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
		return prototype_substitution_identity(
			&ctx->metadata->substitutions,
			&ctx->metadata->contexts,
			refined_branch_context,
			p_substitution
		);
	}
	if (solution->branch_refinement.refinement_status !=
			PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
		return solution->branch_refinement.refinement_status ==
			PROTOTYPE_INDEX_REFINEMENT_INVALID ? -1 : 1;
	}
	const struct prototype_substitution* action = prototype_substitution_get(
		&ctx->metadata->substitutions,
		solution->branch_refinement.substitution
	);
	if (!action || action->source_context != refined_branch_context ||
		solution->branch_refinement.refined_context != refined_branch_context) {
		return -1;
	}
	*p_substitution = solution->branch_refinement.substitution;
	return 0;
}

/* Recover a source-Context candidate from a term in a refined branch. A
 * branch substitution need not be invertible, so this is only candidate
 * generation: reindexing the result through the same substitution must recover
 * the original term before the caller may use it. */
static int operation_solver_match_case_constructor_classifier(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
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
	const struct prototype_typing_constraint* motive_equation =
		operation_solver_motive_equation_for_projection(
			ctx, match_typed_projection, case_index
		);
	uint32_t case_context;
	uint32_t case_projection_substitution;
	int case_context_status = motive_equation ?
		operation_typed_projection_materialized_context(
			ctx,
			motive_equation->payload.classifier.goal.motive_case.
				branch_typed_projection,
			&case_context,
			&case_projection_substitution
		) : -1;
	if (case_context_status != 0) {
		return case_context_status < 0 ? -1 : 1;
	}
	(void)case_projection_substitution;
	/* Constructor specialization belongs to the concrete branch projection.
	 * The occurrence-level case Context is only a source default and may refer
	 * to another projection after function-graph generation. */
	if (case_context == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (source_case->binder_count != operation_case->binder_count ||
		source_case->binder_count > 64 || case_context == PROTOTYPE_INVALID_ID ||
		prototype_term_constructor(
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
			&ctx->metadata->contexts, case_context,
			operation_case->binder_ids[i], &binding_context
			) != 0 || !(binding_entry = prototype_context_get(
				&ctx->metadata->contexts, binding_context
			))) {
			return -1;
		}
		argument_classifiers[i] = operation_context_classifier_term(
			ctx, binding_context
		);
	}
	int saturated;
	int status = operation_constructor_spine_classifier(
		ctx,
		case_context, *p_constructor,
		operation_case->constructor_owner, argument_classifiers,
		source_case->binder_count, p_classifier, &saturated
	);
	if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL &&
		(status != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID || !saturated)) {
		fprintf(
			stderr,
			"motive constructor pending operation=%u projection=%u case=%u "
			"context=%u owner=%u ordinal=%u fields=%u status=%d saturated=%d "
			"classifier=%u\n",
			match_operation_id, match_typed_projection, case_index, case_context,
			operation_case->constructor_owner, source_case->constructor_id,
			source_case->binder_count, status, saturated, *p_classifier
		);
		fprintf(stderr, "motive constructor term ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms, *p_constructor
		);
		fputc('\n', stderr);
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			fprintf(stderr, "motive constructor field[%u] classifier ", i);
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				argument_classifiers[i]
			);
			fputc('\n', stderr);
		}
	}
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
	if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, value_classifier, &type_id, arguments, &argument_count
		) != 0 || type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return 1;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (declaration->index_count > 32 ||
		declaration->parameter_count + declaration->index_count != argument_count) {
		return 1;
	}
	uint32_t index_binders[32];
	uint32_t motive_body = result_classifier;
	uint32_t abstracted_value = value;
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
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms,
				abstracted_value,
				arguments[declaration->parameter_count + i],
				index_variable,
				&abstracted_value
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
			ctx->terms, motive_body,
			abstracted_value, value_variable, &motive_body
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
		uint32_t expected_inner = expected_classifier;
		if (expected_whnf < ctx->terms->term_count &&
			ctx->terms->terms[expected_whnf].tag == PROTOTYPE_TERM_THUNK_TYPE) {
			expected_inner =
				ctx->terms->terms[expected_whnf].as.thunk_type.computation;
		} else if (expected_whnf < ctx->terms->term_count &&
			ctx->terms->terms[expected_whnf].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE) {
			const struct prototype_term* expected =
				&ctx->terms->terms[expected_whnf];
			int purity = prototype_classifier_effect_row_purity(
				ctx->terms, expected->as.computation_type.label
			);
			if (purity == PROTOTYPE_EFFECT_ROW_PURITY_INVALID) {
				return -1;
			}
			if (purity != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
				return 1;
			}
			/* Reverse APP propagation observes the result of forcing the recursive
			 * field. A pure Comp wrapper and the field's Thunk wrapper therefore
			 * meet at the same callable computation; effectful wrappers remain
			 * opaque to compile-time motive synthesis. */
			expected_inner = expected->as.computation_type.result;
		}
		if (expected_inner < ctx->terms->term_count &&
			ctx->terms->terms[expected_inner].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			expected_inner =
				ctx->terms->terms[expected_inner].as.thunk_type.computation;
		}
		return operation_solver_abstract_recursive_field_motive_inner(
			ctx, field->as.thunk_type.computation, field_value,
			expected_inner, depth + 1, p_motive
		);
	}
	if (field->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		struct prototype_classifier_view expected;
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
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			expected.computation_kind ==
				PROTOTYPE_COMPUTATION_KIND_RETURNING) {
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
				expected_classifier, depth + 1, p_motive
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
				ctx->terms, field_body,
				field_binder, shared_value, &next_field
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, expected_body,
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
	int trace = getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL;
	if (trace) {
		fprintf(stderr,
			"ih-field-classifier operation=%u owner=%u case=%u field=%u "
			"binders=%u context=%u action=%u constructor=%u:%u\n",
			operation_id, operation->ih_owner_occurrence,
			operation->ih_case_index, operation->ih_field_index,
			match_case->binder_count, operation->context_id,
			operation->context_action_substitution,
			match_case->constructor_owner, match_case->constructor_id);
	}
	uint32_t field_context = operation->context_id;
	uint32_t field_action = PROTOTYPE_INVALID_ID;
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			operation->context_action_substitution
		);
		uint32_t materialized_context = operation_occurrence_materialized_context(
			ctx, operation_id
		);
		if (!action || materialized_context == PROTOTYPE_INVALID_ID ||
			action->source_context != materialized_context) {
			if (trace) {
				fprintf(stderr,
					"ih-field-classifier failed stage=action operation=%u "
					"materialized=%u action-source=%u action-target=%u\n",
					operation_id, materialized_context,
					action ? action->source_context : PROTOTYPE_INVALID_ID,
					action ? action->target_context : PROTOTYPE_INVALID_ID);
			}
			return -1;
		}
		field_context = action->target_context;
		field_action = operation->context_action_substitution;
	}
	uint32_t classifier;
	struct prototype_context_classifier_view context_view =
		operation_type_context_view(ctx);
	int field_status = prototype_judgement_constructor_field_classifier_in_view(
			ctx->terms, ctx->type_declarations,
			&context_view, &ctx->metadata->substitutions,
			field_context, match_case->constructor_owner,
			match_case->constructor_id,
			&ctx->terms->case_binders[match_case->first_binder],
			operation->ih_field_index, operation->ih_field_index,
			&classifier
		);
	int reindex_status = field_status == 0 && field_action != PROTOTYPE_INVALID_ID ?
		prototype_term_reindex(
			ctx->terms, ctx->type_declarations,
			&ctx->metadata->contexts, &ctx->metadata->substitutions,
			classifier, field_action, &classifier
		) : 0;
	int branch_reindex_status = field_status == 0 && reindex_status == 0 ?
		operation_solver_reindex_in_enclosing_indexed_branches(
			ctx, operation_id, operation->context_id, classifier, &classifier
		) : 0;
	if (field_status != 0 || reindex_status != 0 || branch_reindex_status != 0) {
		if (trace) {
			fprintf(stderr,
				"ih-field-classifier failed stage=classifier operation=%u "
				"field-status=%d reindex-status=%d branch-reindex-status=%d "
				"field-context=%u action=%u\n",
				operation_id, field_status, reindex_status, branch_reindex_status,
				field_context, field_action);
		}
		return field_status < 0 || reindex_status < 0 ||
			branch_reindex_status < 0 ? -1 : 1;
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
	for (uint32_t i = 0; i < ctx->typing->bindings.owner_count; ++i) {
		struct prototype_typing_binder_owner* owner =
			&ctx->typing->bindings.owners[i];
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
			uint64_t normalization_limit = compile_effort_remaining(ctx->metadata);
			int effort_status = compile_effort_charge(
				ctx->metadata, ctx->effort_atomic, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
			);
			if (effort_status != 0) return effort_status;
			int projection_status = prototype_term_project_structural_return_value(
				ctx->terms,
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
			struct prototype_classifier_view computation_view;
			if (prototype_judgement_classifier_view(
					ctx->terms, ctx->type_declarations, NULL,
					computation_classifier, &computation_view
				) != 0 || computation_view.category !=
					PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
				computation_view.computation_kind !=
					PROTOTYPE_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			int purity = prototype_classifier_effect_row_purity(
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
				ctx->terms, projected,
				owner->binding_id,
				value,
				&projected
			) != 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			fprintf(
				stderr,
				"sequence-projection binding=%u producer=%u value=%u result=%u\n",
				owner->binding_id,
				computation_occurrence,
				value,
				projected
			);
		}
	}
	*p_projected = projected;
	return 0;
}

struct operation_motive_scope_query {
	const struct compile_context* ctx;
	const struct prototype_typed_occurrence* owner;
};

static int operation_motive_binding_is_out_of_scope(
	void* context,
	uint32_t binding_id
) {
	const struct operation_motive_scope_query* query = context;
	if (!query || !query->ctx || !query->owner) {
		return -1;
	}
	if (prototype_context_contains_binding(
			&query->ctx->metadata->contexts,
			query->owner->context_id,
			binding_id
		)) {
		return 0;
	}
	for (uint32_t row = 0;
		row < query->owner->implicit_effect_row_count; ++row) {
		if (query->owner->implicit_effect_row_binders[row] == binding_id) {
			return 0;
		}
	}
	return 1;
}

static int operation_solver_seed_indexed_motive_from_ih_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	int* p_changed
) {
	int trace = getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL;
	if (!ctx || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		typed_projection >=
			ctx->typing->context_projections.typed_projection_count ||
		expected_classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	uint32_t argument_occurrence;
	uint32_t argument_typed_projection;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		operation->ih_owner_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
			&argument_occurrence
		) != 0 || operation_solver_projected_child(
			ctx,
			typed_projection,
			PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT,
			0,
			&argument_typed_projection
		) != 0) {
		if (trace) {
			fprintf(stderr,
				"ih-expected-seed failed stage=argument-projection operation=%u "
				"projection=%u tag=%d owner=%u\n",
				operation_id, typed_projection, operation->tag,
				operation->ih_owner_occurrence);
		}
		return -1;
	}
	uint32_t owner_match = operation->ih_owner_occurrence;
	const struct prototype_typing_constraint* ih_equation =
		operation_motive_ih_equation_for_projection_const(
			ctx, typed_projection
		);
	if (!ih_equation || ih_equation->operand_count < 2) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=equation operation=%u projection=%u\n",
				operation_id, typed_projection);
		}
		return -1;
	}
	uint32_t owner_match_typed_projection =
		operation_solver_constraint_operand_projection(ctx, ih_equation, 1);
	const struct prototype_typed_projection* owner_projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections,
			owner_match_typed_projection
		);
	if (!owner_projection || owner_projection->source_occurrence != owner_match) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=owner-projection operation=%u projection=%u owner=%u owner-projection=%u\n",
				operation_id, typed_projection, owner_match,
				owner_match_typed_projection);
		}
		return -1;
	}
	uint32_t argument_classifier = operation_solver_classifier_for_projection(
		ctx, argument_typed_projection
	);
	if (argument_classifier == PROTOTYPE_INVALID_ID) {
		if (trace) {
			fprintf(
				stderr,
				"ih-expected-seed waiting stage=argument-classifier "
				"operation=%u projection=%u argument=%u argument-projection=%u\n",
				operation_id, typed_projection, argument_occurrence,
				argument_typed_projection
			);
		}
		return 1;
	}
	if (operation_solver_term_in_projection_context(
			ctx,
			argument_typed_projection,
			argument_classifier,
			&argument_classifier
		) != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=argument-classifier-context operation=%u projection=%u argument-projection=%u classifier=%u\n",
				operation_id, typed_projection, argument_typed_projection,
				argument_classifier);
		}
		return -1;
	}
	uint32_t result_classifier = expected_classifier;
	uint32_t owner_branch_occurrence;
	struct prototype_classifier_view expected_view;
	int expected_view_status = prototype_judgement_classifier_view(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		expected_classifier,
		&expected_view
	);
	if (expected_view_status != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=expected-view operation=%u projection=%u expected=%u status=%d\n",
				operation_id, typed_projection, expected_classifier,
				expected_view_status);
		}
		return -1;
	}
	if (expected_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION) {
		uint32_t empty_effect_row;
		if (prototype_term_effect_row_empty(
				ctx->terms, &empty_effect_row
			) != 0 || prototype_term_computation_type(
				ctx->terms,
				empty_effect_row,
				expected_classifier,
				PROTOTYPE_COMPUTATION_TOTALITY_TOTAL,
				&result_classifier
			) != 0) {
			return -1;
		}
	}
	if (compile_occurrence_child(
			ctx, owner_match, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
			operation->ih_case_index, &owner_branch_occurrence
		) != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=owner-branch operation=%u projection=%u owner=%u case=%u\n",
				operation_id, typed_projection, owner_match,
				operation->ih_case_index);
		}
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
			result_classifier, &expected_view
		) == 0 && expected_view.category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
		expected_view.computation_kind ==
			PROTOTYPE_COMPUTATION_KIND_RETURNING) {
		result_classifier = expected_view.result;
	}
	int projection_status = operation_solver_project_pure_sequence_results(
		ctx, result_classifier, &result_classifier
	);
	if (projection_status != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed waiting stage=sequence-results operation=%u projection=%u result=%u status=%d\n",
				operation_id, typed_projection, result_classifier,
				projection_status);
		}
		return projection_status;
	}
	uint32_t argument_term;
	if (operation_solver_term_in_projection_context(
			ctx,
			argument_typed_projection,
			ctx->metadata->typed_occurrences.occurrences[
				argument_occurrence
			].core_term,
			&argument_term
		) != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed failed stage=argument-term-context operation=%u projection=%u argument-projection=%u\n",
				operation_id, typed_projection, argument_typed_projection);
		}
		return -1;
	}
	uint32_t motive;
	uint32_t recursive_field_classifier = PROTOTYPE_INVALID_ID;
	int status = operation_solver_abstract_indexed_motive(
		ctx,
		argument_term,
		argument_classifier,
		result_classifier,
		&motive
	);
	if (status > 0) {
		int field_status = operation_solver_induction_field_classifier(
			ctx, operation_id, &recursive_field_classifier
		);
		if (field_status != 0) {
			if (trace) {
				fprintf(stderr,
					"ih-expected-seed failed stage=field-classifier operation=%u "
					"projection=%u status=%d\n",
					operation_id, typed_projection, field_status);
			}
			return -1;
		}
		status = operation_solver_abstract_recursive_field_motive(
			ctx,
			recursive_field_classifier,
			argument_term,
			result_classifier,
			&motive
		);
	}
	if (status != 0) {
		if (trace) {
			fprintf(stderr, "ih-expected-seed waiting stage=abstract operation=%u projection=%u argument=%u result=%u status=%d\n",
				operation_id, typed_projection, argument_term,
				result_classifier, status);
			fprintf(stderr, "ih-expected-field-classifier ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				recursive_field_classifier
			);
			fprintf(stderr, "\nih-expected-argument ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				argument_term
			);
			fprintf(stderr, "\nih-expected-result-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				result_classifier
			);
			fputc('\n', stderr);
		}
		return status;
	}
	if (trace) {
		fprintf(stderr,
			"ih-expected-motive operation=%u projection=%u owner=%u "
			"owner-projection=%u argument=%u argument-classifier=%u "
			"expected=%u result=%u motive=%u\n",
			operation_id, typed_projection, owner_match,
			owner_match_typed_projection, argument_term, argument_classifier,
			expected_classifier, result_classifier, motive);
		fprintf(stderr, "ih-expected-result ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
			result_classifier
		);
		fprintf(stderr, "\nih-expected-motive-term ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms, motive
		);
		fprintf(stderr, "\n");
	}
	const struct prototype_typed_occurrence* owner =
		&ctx->metadata->typed_occurrences.occurrences[owner_match];
	struct operation_motive_scope_query scope_query = {
		.ctx = ctx,
		.owner = owner
	};
	int out_of_scope;
	if (prototype_term_any_free_binding(
			ctx->terms,
			motive,
			operation_motive_binding_is_out_of_scope,
			&scope_query,
			&out_of_scope
		) != 0) {
		return -1;
	}
	if (out_of_scope) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(stderr,
				"ih-expected-motive-out-of-scope operation=%u projection=%u "
				"owner=%u owner-projection=%u\n",
				operation_id, typed_projection, owner_match,
				owner_match_typed_projection);
		}
		/* A motive is rooted outside every branch continuation. Reject an
		 * equation that accidentally captures a branch-local APP or sequence
		 * binder and wait for the enclosing constructor equation to abstract it. */
		return 1;
	}
	uint32_t previous_motive =
		ctx->typing->constraints.motive_solution_for_projection[
			owner_match_typed_projection
		].motive;
	int same_motive = 0;
	if (previous_motive < ctx->terms->term_count &&
		prototype_term_core_shape_equal_for_link(
			ctx->terms,
			previous_motive,
			ctx->terms,
			motive,
			&same_motive
		) != 0) {
		return -1;
	}
	uint8_t motive_state =
		ctx->typing->constraints.motive_solution_for_projection[
			owner_match_typed_projection
		].phase;
	int motive_has_joint_equations =
		motive_state == OPERATION_MOTIVE_PHASE_EQUATION_SET ||
		motive_state == OPERATION_MOTIVE_PHASE_MATERIALIZED;
	if (motive_has_joint_equations && !same_motive) {
		int recursive_equation_counted =
			operation_motive_has_recursive_equations(
				ctx, owner_match, owner_match_typed_projection
			);
		if (recursive_equation_counted < 0) {
			return -1;
		}
		const struct prototype_typing_constraint* current_ih_equation =
			operation_motive_ih_equation_for_projection_const(
				ctx, typed_projection
			);
		int refining_current_equation = current_ih_equation &&
			operation_motive_ih_equation_is_active(
				ctx, current_ih_equation->id, owner_match
			);
		if (recursive_equation_counted > 0 && !refining_current_equation) {
			/* A second recursive consumer contributes another equation for the
			 * selected motive. It cannot replace the first recursive equation. */
			return 1;
		}
		/* Branch equations alone are provisional while this owner still has an
		 * unconsumed IH edge. The first recursive equation may replace that
		 * candidate. The same recursive equation may also become more precise when
		 * a downstream APP determines its generated continuation binder. */
	}
	if (!same_motive || !motive_has_joint_equations) {
		/* IH consumer classifiers are provisional equations until the owner motive
		 * validates every branch. Keep all of them available so multiple recursive
		 * calls can join in one fixed-point round; materialization later replaces
		 * each projection from the selected motive and rejects any mismatch. */
		if (operation_motive_clear_recursive_equations(
				ctx, owner_match, owner_match_typed_projection
			) != 0) {
			return -1;
		}
		struct prototype_typing_motive_solution next =
			ctx->typing->constraints.motive_solution_for_projection[
				owner_match_typed_projection
			];
		next.motive = motive;
		next.status = OPERATION_MOTIVE_STATUS_WAITING;
		next.phase = OPERATION_MOTIVE_PHASE_EQUATION_SET;
		if (operation_motive_solution_transition_for_projection(
				ctx, owner_match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_RECURSIVE_EQUATION, 1, p_changed
			) != 0) {
			return -1;
		}
	}
	return operation_motive_register_recursive_equation(
		ctx, owner_match, owner_match_typed_projection, typed_projection
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
	uint32_t match_typed_projection,
	uint32_t source_case_index,
	uint32_t classifier,
	struct operation_motive_candidate* p_candidate
) {
	int trace = getenv("A_PROGRAM_CONSTRUCTOR_PROVISIONAL_TRACE") != NULL;
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
	uint32_t scrutinee_typed_projection;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		compile_occurrence_child(
			ctx, operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0 ||
		match_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_operation->core_term].tag !=
			PROTOTYPE_TERM_MATCH ||
		source_case_index >= match_operation->case_count ||
		operation_solver_projected_child(
			ctx, match_typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_typed_projection
		) != 0) {
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
	if (source_operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		source_operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u no-constructor\n",
				operation, match_typed_projection);
		}
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}

	uint32_t constructor_term;
	uint32_t argument_classifiers[64];
	int constructor_status = prototype_term_constructor(
		ctx->terms, source_operation_case->constructor_owner,
		source_operation_case->constructor_id, &constructor_term
	);
	if (constructor_status != 0) {
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
		argument_classifiers[i] = operation_context_classifier_term(
			ctx, binding_context
		);
	}
	uint32_t constructor_classifier;
	int constructor_saturated;
	if (operation_constructor_spine_classifier(
			ctx,
			source_operation_case->context_id, constructor_term,
			source_operation_case->constructor_owner, argument_classifiers,
			source_case->binder_count, &constructor_classifier,
			&constructor_saturated
		) != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
		!constructor_saturated) {
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u constructor-pending\n",
				operation, match_typed_projection);
		}
		return 1;
	}

	uint32_t scrutinee_classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_typed_projection
	);
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	uint32_t constructor_type;
	uint32_t constructor_arguments[32];
	uint32_t constructor_argument_count;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) != 0 || prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, constructor_classifier, &constructor_type,
			constructor_arguments, &constructor_argument_count
		) != 0 || scrutinee_type != constructor_type ||
		scrutinee_argument_count != constructor_argument_count ||
		scrutinee_type >= ctx->type_declarations->semantic_schema.type_count) {
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u instance-mismatch scrutinee=%u constructor=%u\n",
				operation, match_typed_projection, scrutinee_classifier,
				constructor_classifier);
		}
		return 1;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type];
	if (declaration->index_count == 0 ||
		declaration->parameter_count + declaration->index_count !=
		constructor_argument_count || declaration->index_count > 32) {
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u non-indexed indices=%u parameters=%u arguments=%u\n",
				operation, match_typed_projection, declaration->index_count,
				declaration->parameter_count, constructor_argument_count);
		}
		return 1;
	}

	struct operation_motive_index_correspondence correspondences[32];
	for (uint32_t i = 0; i < declaration->index_count; ++i) {
		int correspondence_status = operation_motive_branch_index_correspondence(
			ctx,
			operation,
			match_typed_projection,
			source_case_index,
			constructor_classifier,
			i,
			&correspondences[i]
		);
		if (correspondence_status != OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE) {
			if (trace) {
				fprintf(stderr,
					"indexed-branch-seed operation=%u projection=%u index=%u correspondence=%d\n",
					operation, match_typed_projection, i,
					correspondence_status);
			}
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
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u quote=%d classifier=%u substitution=%u\n",
				operation, match_typed_projection, quote_status, classifier,
				correspondences[0].refinement_substitution);
		}
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
			ctx->terms, motive_body,
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
		if (trace) {
			fprintf(stderr,
				"indexed-branch-seed operation=%u projection=%u reapply-mismatch motive=%u classifier=%u\n",
				operation, match_typed_projection, motive_body, classifier);
		}
		return 1;
	}
	p_candidate->motive = motive_body;
	p_candidate->dependency_mask = dependency_mask;
	if (trace) {
		fprintf(stderr,
			"indexed-branch-seed operation=%u projection=%u motive=%u ready\n",
			operation, match_typed_projection, motive_body);
	}
	return 0;
}

static int operation_solver_seed_motive(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t match_typed_projection,
	uint32_t classifier,
	uint32_t source_body_occurrence,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	int recursive_equation_status = operation_motive_has_recursive_equations(
		ctx, operation, match_typed_projection
	);
	if (recursive_equation_status < 0) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
			fprintf(stderr, "motive seed recursive lookup invalid match=%u\n", operation);
		}
		return -1;
	}
	if (recursive_equation_status > 0 &&
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		].motive <
			ctx->terms->term_count) {
		/* A guarded recursive equation determines the indexed family. A single
		 * constructor branch is only a provisional seed and cannot replace that
		 * jointly validated equation, either before or after materialization. */
		return 0;
	}
	const struct prototype_typing_constraint* source_equation = NULL;
	uint32_t source_case_index = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0;
		case_index < ctx->metadata->typed_occurrences.occurrences[operation].case_count;
		++case_index) {
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation) {
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
				fprintf(stderr, "motive seed equation missing match=%u case=%u\n",
					operation, case_index);
			}
			return -1;
		}
		if (equation->payload.classifier.goal.motive_case.branch_operation == source_body_occurrence) {
			source_equation = equation;
			source_case_index = case_index;
			break;
		}
	}
	if (!source_equation) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
			fprintf(stderr, "motive seed source missing match=%u source=%u\n",
				operation, source_body_occurrence);
		}
		return -1;
	}
	struct prototype_typing_motive_solution* solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (solution->phase == OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE &&
		solution->source_case_index == source_case_index &&
		solution->source_classifier == classifier) {
		return 0;
	}
	uint32_t classifier_whnf;
	int capture_status = operation_classifier_captures_case_binder(
		ctx, source_equation, classifier, &classifier_whnf
	);
	if (capture_status < 0) {
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
			fprintf(stderr, "motive seed capture invalid match=%u case=%u\n",
				operation, source_case_index);
		}
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
		ctx, operation, match_typed_projection, source_case_index, classifier,
		&indexed_candidate
	);
	if (indexed_status < 0 && getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
		fprintf(
			stderr,
			"indexed motive seed invalid match=%u projection=%u case=%u "
			"classifier=%u\n",
			operation,
			match_typed_projection,
			source_case_index,
			classifier
		);
	}
	if (indexed_status == OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE ||
		indexed_status == OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT) {
		struct prototype_typing_motive_solution next = *solution;
		next.unavailable_mask |= indexed_candidate.unavailable_mask;
		next.conflict_mask |= indexed_candidate.conflict_mask;
		next.status = OPERATION_MOTIVE_STATUS_WAITING;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION, 0, p_changed
			) != 0) {
			return -1;
		}
		solution = &ctx->typing->constraints.
			motive_solution_for_projection[match_typed_projection];
	}
	if (indexed_status < 0 || (indexed_status > 0 && capture_status > 0)) {
		return indexed_status < 0 ? -1 : 1;
	}
	if (indexed_status == 0) {
		uint32_t indexed_motive = indexed_candidate.motive;
		uint32_t previous_motive =
			ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			].motive;
		int same_motive = 0;
		if (previous_motive < ctx->terms->term_count &&
			prototype_term_core_shape_equal_for_link(
				ctx->terms,
				previous_motive,
				ctx->terms,
				indexed_motive,
				&same_motive
			) != 0) {
			return -1;
		}
		if (ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			].phase ==
				OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE && !same_motive) {
			struct prototype_typing_motive_solution contradiction = *solution;
			contradiction.status = OPERATION_MOTIVE_STATUS_CONTRADICTION;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &contradiction,
					OPERATION_MOTIVE_TRANSITION_CONTRADICTION, 1, p_changed
				) != 0) {
				return -1;
			}
			return 1;
		}
		if (ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			].phase !=
				OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE) {
			struct prototype_typing_motive_solution next = *solution;
			next.motive = indexed_motive;
			next.dependency_mask = indexed_candidate.dependency_mask;
			next.unavailable_mask = indexed_candidate.unavailable_mask;
			next.conflict_mask = indexed_candidate.conflict_mask;
			next.source_case_index = indexed_candidate.source_case_index;
			next.source_classifier = classifier;
			next.status = OPERATION_MOTIVE_STATUS_WAITING;
			next.phase = OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &next,
					OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION, 1, p_changed
				) != 0) {
				return -1;
			}
		} else {
			struct prototype_typing_motive_solution next = *solution;
			next.dependency_mask |= indexed_candidate.dependency_mask;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &next,
					OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION, 0, p_changed
				) != 0) {
				return -1;
			}
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
	uint32_t constructor_term = PROTOTYPE_INVALID_ID;
	int constructor_ready =
		source_operation_case->constructor_owner != PROTOTYPE_INVALID_ID &&
		source_operation_case->constructor_id != PROTOTYPE_INVALID_ID;
	/* Without the constructor correspondence, a nullary branch cannot tell a
	 * constant motive from one that mentions the matched value through the
	 * constructor result (for example n |-> Acc n at zero). Wait for the
	 * branch-refinement answer before making either claim. */
	if (!constructor_ready) {
		return 1;
	}
	if (constructor_ready && prototype_term_constructor(
			ctx->terms,
			source_operation_case->constructor_owner,
			source_operation_case->constructor_id,
			&constructor_term
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; constructor_ready && i < source_case->binder_count; ++i) {
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
	if (constructor_ready && motive_binding != PROTOTYPE_INVALID_ID &&
		prototype_term_var(ctx->terms, motive_binding, &motive_variable) == 0 &&
		prototype_term_graph_replace_exact(
			ctx->terms, classifier,
			constructor_term, motive_variable, &motive_body
		) == 0 && motive_body != classifier &&
		prototype_term_graph_substitute_bound_var(
			ctx->terms, motive_body, motive_binding,
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
		if (ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			].motive != motive ||
			ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			].phase !=
				OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE) {
			struct prototype_typing_motive_solution next =
				ctx->typing->constraints.motive_solution_for_projection[
					match_typed_projection
				];
			next.motive = motive;
			next.dependency_mask = UINT64_C(1);
			next.source_case_index = source_case_index;
			next.source_classifier = classifier;
			next.status = OPERATION_MOTIVE_STATUS_WAITING;
			next.phase = OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &next,
					OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION, 1, p_changed
				) != 0) {
				return -1;
			}
		}
		return 0;
	}
	uint32_t previous =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		].constant_candidate;
	if (previous == PROTOTYPE_INVALID_ID) {
		struct prototype_typing_motive_solution next =
			ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			];
		next.constant_candidate = classifier_whnf;
		next.dependency_mask = 0;
		next.status = OPERATION_MOTIVE_STATUS_WAITING;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION, 1, p_changed
			) != 0) {
			return -1;
		}
		/* Only a branch with an explicit Match/IH back-edge needs a provisional
		 * seed. The immutable dependency slice, rather than the current solved
		 * state, is authoritative here: waiting for a solved recursive equation
		 * would make the seed depend on itself. Ordinary sibling branches remain
		 * independent synthesis premises. */
		for (uint32_t case_index = 0;
			case_index < match_operation->case_count; ++case_index) {
			const struct prototype_typing_constraint* equation =
				operation_solver_motive_equation_for_projection(
					ctx, match_typed_projection, case_index
				);
			if (!equation) {
				return -1;
			}
			if (ctx->typing->constraints.branch_ih_dependency_count[
					equation->id
				] == 0) {
				continue;
			}
			uint32_t branch_projection = equation->payload.classifier.goal.
				motive_case.branch_typed_projection;
			if (case_index == source_case_index ||
				operation_solver_classifier_for_projection(
					ctx, branch_projection
				) != PROTOTYPE_INVALID_ID) {
				continue;
			}
			int refine_status =
				operation_refine_projected_classifier_from_expected(
					ctx, branch_projection, classifier_whnf,
					equation->id, 0, p_changed
				);
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(
					stderr,
					"motive-constant-propagate match=%u projection=%u "
					"source-case=%u target-case=%u branch=%u expected=%u status=%d\n",
					operation, match_typed_projection, source_case_index,
					case_index, branch_projection, classifier_whnf,
					refine_status
				);
			}
			if (refine_status < 0) {
				struct prototype_typing_motive_solution invalid =
					ctx->typing->constraints.
						motive_solution_for_projection[
							match_typed_projection
						];
				invalid.constant_candidate = PROTOTYPE_INVALID_ID;
				invalid.status = OPERATION_MOTIVE_STATUS_WAITING;
				if (operation_motive_solution_transition_for_projection(
						ctx, match_typed_projection, &invalid,
						OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION,
						1, p_changed
					) != 0) {
					return -1;
				}
				return 1;
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
			struct prototype_typing_motive_solution contradiction =
				ctx->typing->constraints.motive_solution_for_projection[
					match_typed_projection
				];
			contradiction.status = OPERATION_MOTIVE_STATUS_CONTRADICTION;
			if (operation_motive_solution_transition_for_projection(
					ctx, match_typed_projection, &contradiction,
					OPERATION_MOTIVE_TRANSITION_CONTRADICTION, 1, p_changed
				) != 0) {
				return -1;
			}
		}
		return join_status < 0 ? -1 : 1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, joined
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		next.constant_candidate = joined;
	}
	return operation_motive_solution_transition_for_projection(
		ctx, match_typed_projection, &next,
		OPERATION_MOTIVE_TRANSITION_BRANCH_EQUATION,
		1, p_changed
	);
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
	return 0;
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
				quoted,
				image,
				source_variable,
				&next
			) != 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_MOTIVE_SUBSTITUTION_TRACE") && next != quoted) {
			fprintf(stderr,
				"motive-quote-rewrite substitution=%u binding=%u image=%u "
				"source=%u before=%u after=%u\n",
				substitution_id, entry->binding_id, image, source_variable,
				quoted, next);
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
	uint32_t match_typed_projection,
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
	uint32_t scrutinee_typed_projection;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		case_index >= match_operation->case_count || compile_occurrence_child(
			ctx, match_operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0 || operation_solver_projected_child(
			ctx, match_typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_typed_projection
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t case_projection_id;
	if (prototype_typed_match_case_projection_find(
			&ctx->typing->context_projections, operation_case_id,
			match_typed_projection, &case_projection_id
		) != 0 || case_projection_id >=
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t refinement_constraint_id = ctx->typing->constraints.
		branch_refinement_constraint_for_match_case_projection[
			case_projection_id
		];
	if (refinement_constraint_id >= ctx->typing->constraints.constraint_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}
	const struct prototype_typing_constraint* refinement_constraint =
		&ctx->typing->constraints.constraints[refinement_constraint_id];
	const struct prototype_typing_constraint_solution* refinement_solution =
		&ctx->typing->constraints.solutions[refinement_constraint_id];
	if (refinement_constraint->domain !=
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
		refinement_constraint->payload.branch_refinement.match_case_projection !=
			case_projection_id) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	if (refinement_solution->branch_refinement.refinement_status !=
			PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}
	const struct prototype_substitution* refinement = prototype_substitution_get(
		&ctx->metadata->substitutions,
		refinement_solution->branch_refinement.substitution
	);
	uint32_t materialized_case_context =
		refinement_solution->branch_refinement.refined_context;
	const struct prototype_typed_projection* owner_projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, match_typed_projection
		);
	uint32_t materialized_match_context;
	uint32_t owner_substitution;
	int projection_status = operation_typed_projection_materialized_context(
		ctx, match_typed_projection, &materialized_match_context,
		&owner_substitution
	);
	if (!owner_projection || owner_projection->source_occurrence !=
			match_operation_id) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	if (projection_status > 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}
	if (projection_status < 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	if (!refinement || materialized_case_context == PROTOTYPE_INVALID_ID ||
		materialized_match_context == PROTOTYPE_INVALID_ID ||
		refinement->source_context != materialized_case_context ||
		!prototype_context_is_ancestor(
			&ctx->metadata->contexts,
			materialized_match_context,
			refinement->target_context
		)) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}

	uint32_t scrutinee_classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_typed_projection
	);
	uint32_t family_type_id;
	uint32_t source_arguments[32];
	uint32_t source_argument_count;
	uint32_t constructor_type_id;
	uint32_t branch_arguments[32];
	uint32_t branch_argument_count;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier, &family_type_id,
			source_arguments, &source_argument_count
		) != 0 || prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, constructor_classifier, &constructor_type_id,
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
	struct prototype_context_classifier_view context_view =
		operation_type_context_view(ctx);
	int parameter_status = prototype_context_substitution_from_terms_in_view(
			&context_view,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			materialized_match_context,
			declaration->parameter_context,
			source_arguments,
			declaration->parameter_count,
			&parameter_substitution
		);
	if (parameter_status != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index_classifier;
	int index_classifier_status = prototype_context_telescope_entry_classifier_in_view(
			&context_view,
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
		);
	if (index_classifier_status != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index =
		source_arguments[declaration->parameter_count + index_ordinal];
	uint32_t branch_index;
	uint32_t branch_index_classifier;
	int branch_index_status = prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index,
			refinement_solution->branch_refinement.substitution,
			&branch_index
		);
	int branch_classifier_status = prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index_classifier,
			refinement_solution->branch_refinement.substitution,
			&branch_index_classifier
		);
	if (branch_index_status != 0 || branch_classifier_status != 0) {
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
	p_correspondence->branch_context = materialized_case_context;
	p_correspondence->refinement_substitution =
		refinement_solution->branch_refinement.substitution;
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

static const struct prototype_typing_constraint*
operation_motive_ih_equation_for_projection_const(
	const struct compile_context* ctx,
	uint32_t typed_projection
) {
	if (!ctx || !ctx->metadata || typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return NULL;
	}
	uint32_t constraint_id =
		ctx->typing->constraints.ih_constraint_for_projection[typed_projection];
	if (!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return NULL;
	}
	const struct prototype_typing_constraint* constraint =
		&ctx->typing->constraints.constraints[constraint_id];
	return constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
		constraint->owner_typed_projection == typed_projection ?
			constraint : NULL;
}

static struct prototype_typing_constraint* operation_motive_ih_equation_for_projection(
	struct compile_context* ctx,
	uint32_t typed_projection
) {
	if (!ctx || !ctx->metadata || typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return NULL;
	}
	uint32_t constraint_id =
		ctx->typing->constraints.ih_constraint_for_projection[typed_projection];
	if (!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return NULL;
	}
	struct prototype_typing_constraint* constraint =
		&ctx->typing->constraints.constraints[constraint_id];
	return constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
		constraint->owner_typed_projection == typed_projection ?
			constraint : NULL;
}

static const struct prototype_typing_constraint*
operation_motive_ih_equation_for_constraint_const(
	const struct compile_context* ctx,
	uint32_t constraint_id
) {
	if (!ctx || !ctx->metadata || !prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return NULL;
	}
	const struct prototype_typing_constraint* constraint =
		&ctx->typing->constraints.constraints[constraint_id];
	if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED ||
		constraint->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[
			constraint->source_occurrence
		].tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		return NULL;
	}
	return constraint;
}

static int operation_motive_ih_equation_is_active(
	const struct compile_context* ctx,
	uint32_t constraint_id,
	uint32_t owner_match
) {
	const struct prototype_typing_constraint* constraint =
		operation_motive_ih_equation_for_constraint_const(ctx, constraint_id);
	return constraint &&
		ctx->typing->constraints.solutions[constraint->id].activation_revision != 0 &&
		constraint->payload.classifier.goal.induction_hypothesis.
			owner_match_occurrence == owner_match;
}

static int operation_motive_ih_projection_is_current(
	const struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t motive
) {
	const struct prototype_typing_constraint* equation =
		operation_motive_ih_equation_for_projection_const(
			ctx, typed_projection
		);
	if (!equation) {
		return 0;
	}
	return operation_solver_classifier_for_projection(ctx, typed_projection) !=
			PROTOTYPE_INVALID_ID &&
		ctx->typing->constraints.solutions[equation->id].projected_motive == motive;
}

static int operation_motive_publish_ih_projection(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t motive,
	int* p_changed
) {
	struct prototype_typing_constraint* equation =
		operation_motive_ih_equation_for_projection(
		ctx, typed_projection
	);
	if (!equation || motive >= ctx->terms->term_count ||
		(ctx->typing->constraints.solutions[equation->id].result_term ==
				PROTOTYPE_INVALID_ID &&
		 operation_solver_classifier_for_projection(ctx, typed_projection) ==
				PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	struct prototype_typing_constraint_solution* ih_solution =
		&ctx->typing->constraints.solutions[equation->id];
	if (ih_solution->projected_motive == motive) {
		return 0;
	}
	uint32_t aggregate_id;
	if (prototype_typing_constraint_record_semantic_transition(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		) != 0 || operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &aggregate_id
		) != 0) {
		return -1;
	}
	ih_solution->projected_motive = motive;
	ih_solution->answer_revision++;
	/* projected_motive is part of the IH answer's provenance. Sequence equations
	 * name this premise explicitly, so a DefEq-stable classifier still wakes its
	 * consumers when provisional evidence becomes motive-derived evidence. */
	if (operation_solver_enqueue_equation_dependents(ctx, equation->id) != 0 ||
		operation_solver_enqueue_projection_dependents(
			ctx, typed_projection
		) != 0) {
		return -1;
	}
	if (p_changed) {
		*p_changed = 1;
	}
	(void)aggregate_id;
	return 0;
}

static int operation_motive_validate_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	const struct prototype_typing_constraint* motive_equation,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !motive_equation || !p_validation ||
		!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, motive_equation->id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return -1;
	}
	uint32_t first = ctx->typing->constraints.first_branch_ih_dependency[
		motive_equation->id
	];
	uint32_t count = ctx->typing->constraints.branch_ih_dependency_count[
		motive_equation->id
	];
	if (count == 0) {
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	if (first == PROTOTYPE_INVALID_ID || first >
			ctx->typing->constraints.branch_ih_dependency_total || count >
			ctx->typing->constraints.branch_ih_dependency_total - first) {
		return -1;
	}
	for (uint32_t dependency = 0; dependency < count; ++dependency) {
		uint32_t ih_constraint =
			ctx->typing->constraints.branch_ih_dependencies[first + dependency];
		const struct prototype_typing_constraint* ih_equation =
			operation_motive_ih_equation_for_constraint_const(
				ctx, ih_constraint
			);
		if (!ih_equation) return -1;
		uint32_t ih_operation = ih_equation->source_occurrence;
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
			return OPERATION_MOTIVE_EQUATION_MISMATCH;
		}
		/* The Match/IH back-edge is the recursive equation even when no later
		 * expected-type constraint activated this IH. Activation controls the
		 * additional projected-classifier check below, not guarded recursion itself. */
	}
	return 0;
}

static int operation_motive_validate_equation_impl(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
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
	const struct prototype_typing_constraint* equation =
		operation_solver_motive_equation_for_projection(
			ctx, match_typed_projection, case_index
		);
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !equation ||
		equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (operation_case->refinement_status ==
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
		/* An impossible constructor contributes no equation to the motive.
		 * Its surface body is retained for readback, but cannot constrain the
		 * classifier of an unreachable branch. */
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	uint32_t constructor;
	uint32_t constructor_classifier;
	int constructor_status = operation_solver_match_case_constructor_classifier(
		ctx,
		match_operation_id,
		match_typed_projection,
		case_index,
		&constructor,
		&constructor_classifier
	);
	if (constructor_status < 0) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive validation invalid operation=%u projection=%u case=%u "
				"stage=constructor status=%d\n",
				match_operation_id, match_typed_projection, case_index,
				constructor_status
			);
		}
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	if (constructor_status > 0) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive validation pending operation=%u projection=%u case=%u "
				"stage=constructor status=%d\n",
				match_operation_id, match_typed_projection, case_index,
				constructor_status
			);
		}
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	uint32_t actual = operation_solver_classifier_for_projection(
		ctx,
		equation->payload.classifier.goal.motive_case.branch_typed_projection
	);
	if (actual == PROTOTYPE_INVALID_ID) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive validation pending operation=%u projection=%u case=%u "
				"stage=branch-classifier\n",
				match_operation_id, match_typed_projection, case_index
			);
		}
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	int recursive_validation = operation_motive_validate_recursive_equations(
		ctx, match_operation_id, equation, p_validation
	);
	if (recursive_validation != OPERATION_MOTIVE_EQUATION_VALID) {
		return recursive_validation;
	}
	if (p_validation->used_recursive_equation) {
		uint32_t first = ctx->typing->constraints.first_branch_ih_dependency[equation->id];
		uint32_t count = ctx->typing->constraints.branch_ih_dependency_count[equation->id];
		if (first == PROTOTYPE_INVALID_ID || first >
				ctx->typing->constraints.branch_ih_dependency_total || count >
				ctx->typing->constraints.branch_ih_dependency_total - first) {
			return OPERATION_MOTIVE_EQUATION_INVALID;
		}
		for (uint32_t dependency = 0; dependency < count; ++dependency) {
			uint32_t ih_constraint =
				ctx->typing->constraints.branch_ih_dependencies[
					first + dependency
				];
			const struct prototype_typing_constraint* ih_equation =
				operation_motive_ih_equation_for_constraint_const(
					ctx, ih_constraint
				);
			if (!ih_equation) return OPERATION_MOTIVE_EQUATION_INVALID;
			if (!operation_motive_ih_equation_is_active(
					ctx, ih_constraint, match_operation_id
				)) {
				continue;
			}
			if (!operation_motive_ih_projection_is_current(
					ctx, ih_equation->owner_typed_projection, motive
				)) {
				/* First project this candidate to every recursive premise, then
				 * recompute the branch through the ordinary dependency graph. */
				return OPERATION_MOTIVE_EQUATION_PENDING;
			}
		}
	}
	uint32_t branch_motive = motive;
	uint32_t branch_context;
	uint32_t branch_projection_substitution;
	int branch_context_status = operation_typed_projection_materialized_context(
		ctx,
		equation->payload.classifier.goal.motive_case.branch_typed_projection,
		&branch_context,
		&branch_projection_substitution
	);
	if (branch_context_status != 0) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive validation %s operation=%u projection=%u case=%u "
				"stage=branch-context status=%d branch-projection=%u\n",
				branch_context_status < 0 ? "invalid" : "pending",
				match_operation_id, match_typed_projection, case_index,
				branch_context_status,
				equation->payload.classifier.goal.motive_case.branch_typed_projection
			);
		}
		return branch_context_status < 0 ?
			OPERATION_MOTIVE_EQUATION_INVALID :
			OPERATION_MOTIVE_EQUATION_PENDING;
	}
	(void)branch_projection_substitution;
	uint32_t branch_refinement_substitution;
	int refinement_status = operation_motive_case_refinement_substitution(
		ctx, equation, branch_context, &branch_refinement_substitution
	);
	if (refinement_status != 0) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive validation %s operation=%u projection=%u case=%u "
				"stage=branch-refinement status=%d context=%u case-projection=%u\n",
				refinement_status < 0 ? "invalid" : "pending",
				match_operation_id, match_typed_projection, case_index,
				refinement_status, branch_context,
				equation->payload.classifier.goal.motive_case.match_case_projection
			);
		}
		return refinement_status < 0 ?
			OPERATION_MOTIVE_EQUATION_INVALID :
			OPERATION_MOTIVE_EQUATION_PENDING;
	}
	/* The branch motive and constructor must be checked through the same
	 * projection-owned refinement. The occurrence-level accepted case is a
	 * publication view and cannot select a substitution for another projection. */
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			motive,
			branch_refinement_substitution,
			&branch_motive
		) != 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t expected;
	uint32_t normalized_expected;
	int motive_apply_status = prototype_judgement_apply_indexed_motive(
			ctx->terms,
			ctx->type_declarations,
			branch_motive,
			constructor,
			constructor_classifier,
			1,
			&expected
		);
	if (motive_apply_status != 0) {
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(
				stderr,
				"motive application pending operation=%u projection=%u case=%u "
				"motive=%u constructor=%u classifier=%u status=%d\n",
				match_operation_id,
				match_typed_projection,
				case_index,
				branch_motive,
				constructor,
				constructor_classifier,
				motive_apply_status
			);
			fprintf(stderr, "motive application motive-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms, branch_motive
			);
			fprintf(stderr, "\nmotive application classifier-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				constructor_classifier
			);
			fputc('\n', stderr);
		}
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	if (typing_core_normalize_complete(
			ctx->core_requests,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
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
	struct prototype_classifier_view expected_view;
	struct prototype_classifier_view actual_view;
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
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			expected_view.computation_kind ==
				PROTOTYPE_COMPUTATION_KIND_RETURNING ?
				expected_view.result : normalized_expected;
		uint32_t actual_result = actual_view.category ==
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			actual_view.computation_kind ==
				PROTOTYPE_COMPUTATION_KIND_RETURNING ?
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
	if (p_validation->used_recursive_equation) {
		uint32_t validated_expected;
		int guarded_status = prototype_judgement_guarded_classifier_equation(
			ctx->terms,
			ctx->type_declarations,
			normalized_expected,
			actual,
			&validated_expected
		);
		if (guarded_status < 0) {
			return OPERATION_MOTIVE_EQUATION_INVALID;
		}
		if (guarded_status > 0) {
			p_validation->expected_classifier = validated_expected;
			p_validation->actual_classifier = actual;
			p_validation->conversion_status = PROTOTYPE_TERM_CONVERSION_EQUAL;
			return OPERATION_MOTIVE_EQUATION_VALID;
		}
	}
	if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
		fprintf(stderr,
			"motive equation mismatch operation=%u projection=%u case=%u "
			"expected=%u actual=%u\n",
			match_operation_id, match_typed_projection, case_index,
			p_validation->expected_classifier, p_validation->actual_classifier);
		fprintf(stderr, "motive equation expected-term ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
			p_validation->expected_classifier
		);
		fprintf(stderr, "\nmotive equation actual-term ");
		prototype_type_term_print_debug(
			stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
			p_validation->actual_classifier
		);
		fprintf(stderr, "\n");
	}
	return OPERATION_MOTIVE_EQUATION_MISMATCH;
}

/* Validation may intern normalization results in TermDB, but it must not
 * discover constraints, activate recursive edges, or alter solver answers.
 * Keep an exact snapshot here rather than relying on callers to remember a
 * partial counter check. */
static int operation_motive_validate_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t case_index,
	uint32_t motive,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !ctx->metadata) {
		return operation_motive_validate_equation_impl(
			ctx, match_operation_id, match_typed_projection, case_index,
			motive, p_validation
		);
	}
	size_t projection_count =
		ctx->typing->context_projections.typed_projection_count;
	size_t constraint_count = ctx->typing->constraints.constraint_count;
	if (projection_count > SIZE_MAX / sizeof(
			ctx->typing->constraints.motive_solution_for_projection[0]
		) ||
		constraint_count > SIZE_MAX /
			sizeof(ctx->typing->constraints.constraints[0])) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	size_t motive_bytes = projection_count * sizeof(
		ctx->typing->constraints.motive_solution_for_projection[0]
	);
	size_t constraint_bytes = constraint_count *
		sizeof(ctx->typing->constraints.constraints[0]);
	if (motive_bytes > SIZE_MAX - constraint_bytes) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	size_t snapshot_bytes = motive_bytes + constraint_bytes;
	unsigned char* snapshot = malloc(snapshot_bytes == 0 ? 1 : snapshot_bytes);
	if (!snapshot) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	unsigned char* cursor = snapshot;
	memcpy(
		cursor,
		ctx->typing->constraints.motive_solution_for_projection,
		motive_bytes
	);
	cursor += motive_bytes;
	memcpy(cursor, ctx->typing->constraints.constraints, constraint_bytes);

	int status = operation_motive_validate_equation_impl(
		ctx, match_operation_id, match_typed_projection, case_index,
		motive, p_validation
	);
	cursor = snapshot;
	int unchanged = memcmp(
		cursor,
		ctx->typing->constraints.motive_solution_for_projection,
		motive_bytes
	) == 0;
	cursor += motive_bytes;
	unchanged = unchanged && memcmp(
		cursor, ctx->typing->constraints.constraints, constraint_bytes
	) == 0;
	free(snapshot);
	if (!unchanged) {
		fprintf(stderr,
			"motive validation mutated solver state match=%u case=%u\n",
			match_operation_id, case_index);
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	return status;
}

static int operation_motive_collect_owned_induction_hypotheses(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t* constraints,
	uint32_t capacity,
	uint32_t* p_count
) {
	if (!ctx || !ctx->metadata || !constraints || !p_count ||
		match_operation_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return -1;
	}
	uint32_t count = 0;
	for (uint32_t constraint_id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		constraint_id != PROTOTYPE_INVALID_ID;
		constraint_id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, constraint_id
		)) {
		if (!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, constraint_id,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			)) {
			return -1;
		}
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[constraint_id];
		if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED) {
			continue;
		}
		if (constraint->payload.classifier.goal.induction_hypothesis.
				owner_match_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		if (constraint->payload.classifier.goal.induction_hypothesis.
				owner_match_occurrence != match_operation_id) {
			continue;
		}
		if (constraint->operand_count >= 2 &&
			operation_solver_constraint_operand_projection(ctx, constraint, 1) ==
				match_typed_projection) {
			if (count >= capacity) {
				return -1;
			}
			constraints[count++] = constraint_id;
		}
	}
	*p_count = count;
	return 0;
}

static int operation_motive_clear_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection
) {
	uint32_t operations[4096];
	uint32_t operation_count;
	if (operation_motive_collect_owned_induction_hypotheses(
		ctx, match_operation_id, match_typed_projection, operations, 4096,
		&operation_count
		) != 0) {
		return -1;
	}
	int changed = 0;
	for (uint32_t index = 0; index < operation_count; ++index) {
		uint32_t constraint_id = operations[index];
		struct prototype_typing_constraint* constraint =
			prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, constraint_id,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			) ?
			&ctx->typing->constraints.constraints[constraint_id] : NULL;
		if (!constraint || constraint->payload.classifier.goal.
				induction_hypothesis.owner_match_occurrence != match_operation_id) {
			return -1;
		}
		if (ctx->typing->constraints.solutions[constraint->id].activation_revision != 0) {
			ctx->typing->constraints.solutions[constraint->id].activation_revision = 0;
			changed = 1;
		}
	}
	if (changed) {
		struct prototype_typing_motive_solution next =
			ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			];
		next.recursive_equation_revision++;
		next.status = OPERATION_MOTIVE_STATUS_WAITING;
		next.phase = OPERATION_MOTIVE_PHASE_EQUATION_SET;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_RECURSIVE_EQUATION, 0, NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_motive_has_recursive_equations(
	const struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operations[4096];
	uint32_t operation_count;
	if (operation_motive_collect_owned_induction_hypotheses(
		ctx, match_operation_id, match_typed_projection, operations, 4096,
		&operation_count
		) != 0) {
		return -1;
	}
	for (uint32_t index = 0; index < operation_count; ++index) {
		if (operation_motive_ih_equation_is_active(
				ctx, operations[index], match_operation_id
			)) {
			return 1;
		}
	}
	return 0;
}

static int operation_motive_register_recursive_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t ih_typed_projection
) {
	const struct prototype_typed_projection* projection = ctx ?
		prototype_typed_projection_get(
			&ctx->typing->context_projections, ih_typed_projection
		) : NULL;
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count || !projection ||
		projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		].tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		].ih_owner_occurrence != match_operation_id) {
		return -1;
	}
	struct prototype_typing_constraint* ih_equation =
		operation_motive_ih_equation_for_projection(
		ctx, ih_typed_projection
	);
	if (!ih_equation || ih_equation->payload.classifier.goal.
			induction_hypothesis.owner_match_occurrence != match_operation_id ||
		ih_equation->operand_count < 2 ||
		operation_solver_constraint_operand_projection(ctx, ih_equation, 1) != match_typed_projection) {
		return -1;
	}
	if (ctx->typing->constraints.solutions[ih_equation->id].activation_revision == 0) {
		struct prototype_typing_motive_solution next =
			ctx->typing->constraints.motive_solution_for_projection[
				match_typed_projection
			];
		next.recursive_equation_revision++;
		next.status = OPERATION_MOTIVE_STATUS_WAITING;
		next.phase = OPERATION_MOTIVE_PHASE_EQUATION_SET;
		if (operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_RECURSIVE_EQUATION, 0, NULL
			) != 0) {
			return -1;
		}
		ctx->typing->constraints.solutions[ih_equation->id].activation_revision =
			next.recursive_equation_revision;
		const struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			return -1;
		}
		for (uint32_t case_index = 0; case_index < match->case_count;
			++case_index) {
			const struct prototype_typing_constraint* indexed_equation =
				operation_solver_motive_equation_for_projection(
					ctx, match_typed_projection, case_index
				);
			if (!indexed_equation) {
				return -1;
			}
			uint32_t constraint_id = indexed_equation->id;
			struct prototype_typing_constraint* equation =
				&ctx->typing->constraints.constraints[constraint_id];
			if (equation->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION ||
				equation->source_occurrence !=
					match_operation_id || prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					constraint_id,
					OPERATION_CONSTRAINT_STATE_PENDING,
					OPERATION_CLASSIFIER_GOAL_REASON_NONE
				) != 0 || operation_solver_enqueue_constraint(
					ctx, constraint_id, PROTOTYPE_COMPILE_ENQUEUE_MOTIVE
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

/* Diagnostics are emitted only after solving has stopped. Candidate metadata
 * is therefore a write-only explanation channel and never a solver input. */
static int operation_motive_add_terminal_diagnostic(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t match_typed_projection,
	uint32_t case_index
) {
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typing_constraint* equation =
		operation_solver_motive_equation_for_projection(
			ctx, match_typed_projection, case_index
		);
	if (!equation) {
		return -1;
	}
	const struct prototype_typing_motive_solution* solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
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
			ctx, match_operation_id, match_typed_projection, case_index,
			solution->motive, &validation
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
			.span = compile_source_span(ctx, source_ast),
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
	const struct prototype_typing_match_goal* typing,
	uint32_t match_typed_projection,
	const struct prototype_typed_occurrence* operation,
	const struct prototype_term* match,
	const uint32_t* branch_results,
	const uint32_t* branch_constructor_terms,
	const uint32_t* branch_constructor_results,
	const uint32_t* branch_refinement_substitutions,
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
		!branch_refinement_substitutions ||
		!scrutinee_arguments || !p_classifier || index_count > 32 ||
		scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		scrutinee_argument_count != scrutinee_parameter_count + index_count) {
		return 1;
	}
	uint32_t common = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count;
		++case_index) {
		uint32_t branch_substitution =
			branch_refinement_substitutions[case_index];
		if (!prototype_substitution_get(
				&ctx->metadata->substitutions, branch_substitution
			)) {
			return 1;
		}
		struct prototype_term_reduction_options normalization_options = {
			.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
		};
		uint32_t normalized_branch_result;
		uint32_t normalized_branch_constructor;
		if (prototype_term_nf_with_options(
				ctx->terms,
				NULL,
				normalization_options,
				branch_results[case_index],
				&normalized_branch_result
			) != 0 || prototype_term_nf_with_options(
				ctx->terms,
				NULL,
				normalization_options,
				branch_constructor_terms[case_index],
				&normalized_branch_constructor
			) != 0) {
			return -1;
		}
		uint32_t quoted;
		int quote_status = operation_motive_quote_refined_term(
			ctx,
			normalized_branch_result,
			branch_substitution,
			&quoted
		);
		if (quote_status != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-quoted-unavailable operation=%u projection=%u "
					"case=%u stage=branch-result status=%d substitution=%u\n",
					typing->operation, match_typed_projection, case_index,
					quote_status, branch_substitution);
			}
			return quote_status < 0 ? -1 : 1;
		}
		uint32_t quoted_constructor;
		int constructor_quote_status = operation_motive_quote_refined_term(
			ctx,
			normalized_branch_constructor,
			branch_substitution,
			&quoted_constructor
		);
		if (constructor_quote_status != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-quoted-unavailable operation=%u projection=%u "
					"case=%u stage=constructor-quote status=%d substitution=%u\n",
					typing->operation, match_typed_projection, case_index,
					constructor_quote_status, branch_substitution);
			}
			return constructor_quote_status < 0 ? -1 : 1;
		}
		uint32_t normalized_quoted;
		uint32_t normalized_quoted_constructor;
		if (prototype_term_nf_with_options(
				ctx->terms,
				NULL,
				normalization_options,
				quoted,
				&normalized_quoted
			) != 0 || prototype_term_nf_with_options(
				ctx->terms,
				NULL,
				normalization_options,
				quoted_constructor,
				&normalized_quoted_constructor
			) != 0) {
			return -1;
		}
		quoted = normalized_quoted;
		quoted_constructor = normalized_quoted_constructor;
		uint32_t constructor_type = PROTOTYPE_INVALID_ID;
		uint32_t constructor_arguments[64];
		uint32_t constructor_argument_count = 0;
		if (prototype_type_projection_instance_info(
				ctx->terms,
			&ctx->type_declarations->semantic_schema,
				branch_constructor_results[case_index],
				&constructor_type,
				constructor_arguments,
				&constructor_argument_count
			) != 0 || constructor_type != scrutinee_type ||
			constructor_argument_count != scrutinee_argument_count) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-quoted-unavailable operation=%u projection=%u "
					"case=%u stage=constructor-instance type=%u/%u "
					"arguments=%u/%u\n",
					typing->operation, match_typed_projection, case_index,
					constructor_type, scrutinee_type,
					constructor_argument_count, scrutinee_argument_count);
			}
			return 1;
		}
		for (uint32_t index = 0; index < index_count; ++index) {
			uint32_t quoted_index;
			int index_quote_status = operation_motive_quote_refined_term(
				ctx,
				constructor_arguments[scrutinee_parameter_count + index],
				branch_substitution,
				&quoted_index
			);
			uint32_t next_quoted;
			uint32_t next_constructor;
			if (index_quote_status != 0) {
				if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
					fprintf(stderr,
						"motive-quoted-unavailable operation=%u projection=%u "
						"case=%u stage=index-%u status=%d substitution=%u\n",
						typing->operation, match_typed_projection, case_index,
						index, index_quote_status, branch_substitution);
				}
				return index_quote_status < 0 ? -1 : 1;
			}
			if (prototype_term_graph_replace_exact(
					ctx->terms, quoted,
					quoted_index,
					scrutinee_arguments[scrutinee_parameter_count + index],
					&next_quoted
				) != 0 || prototype_term_graph_replace_exact(
					ctx->terms, quoted_constructor,
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
				ctx->terms, quoted,
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
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-quoted-unavailable operation=%u projection=%u "
					"case=%u stage=common quoted=%u common=%u\n",
					typing->operation, match_typed_projection, case_index,
					quoted, common);
				if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE")) {
					fprintf(stderr, "motive-quoted-common-term ");
					prototype_type_term_print_debug(
						stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
						common
					);
					fprintf(stderr, "\nmotive-quoted-branch-term ");
					prototype_type_term_print_debug(
						stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
						quoted
					);
					fprintf(stderr, "\n");
				}
			}
			return 1;
		}
	}
	uint32_t value_binding = prototype_term_new_binding(ctx->terms);
	uint32_t value_variable;
	uint32_t motive_body;
	if (common == PROTOTYPE_INVALID_ID || value_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, value_binding, &value_variable) != 0 ||
		prototype_term_graph_replace_exact(
			ctx->terms, common,
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
				ctx->terms, motive_body,
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
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
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
			) != 0 || typing_core_normalize_complete(
				ctx->core_requests,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				expected_branch,
				&normalized_expected
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				normalized_expected,
				operation_solver_classifier_for_projection(
					ctx,
					equation->payload.classifier.goal.motive_case.
						branch_typed_projection
				)
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-quoted-unavailable operation=%u projection=%u "
					"case=%u stage=reapply motive=%u\n",
					typing->operation, match_typed_projection, case_index, motive);
			}
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

static int operation_motive_replace_computation_envelope(
	struct compile_context* ctx,
	uint32_t motive,
	uint32_t effect_row,
	int totality,
	uint32_t* p_motive
) {
	if (!ctx || !p_motive || motive >= ctx->terms->term_count ||
		effect_row >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t bindings[65];
	uint32_t binding_count = 0;
	uint32_t body = motive;
	while (body < ctx->terms->term_count &&
		ctx->terms->terms[body].tag == PROTOTYPE_TERM_LAMBDA) {
		if (binding_count >= 65) {
			return -1;
		}
		bindings[binding_count++] = ctx->terms->terms[body].as.lambda.binding_id;
		body = ctx->terms->terms[body].as.lambda.body;
	}
	if (body >= ctx->terms->term_count ||
		ctx->terms->terms[body].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 1;
	}
	uint32_t rebuilt;
	if (prototype_term_computation_type(
			ctx->terms,
			effect_row,
			ctx->terms->terms[body].as.computation_type.result,
			totality,
			&rebuilt
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = binding_count; reverse > 0; --reverse) {
		if (prototype_term_lambda(
				ctx->terms, bindings[reverse - 1], rebuilt, &rebuilt
			) != 0) {
			return -1;
		}
	}
	*p_motive = rebuilt;
	return 0;
}

static int build_operation_motive(
	struct compile_context* ctx,
	const struct prototype_typing_match_goal* typing,
	uint32_t match_typed_projection,
	uint32_t* p_classifier,
	uint32_t* p_validated_motive
) {
	if (!ctx || !typing || !p_classifier || !p_validated_motive ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	*p_validated_motive = PROTOTYPE_INVALID_ID;
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
	uint32_t branch_refinement_substitutions[64];
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
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
		uint32_t branch_classifier = operation_solver_classifier_for_projection(
			ctx,
			equation->payload.classifier.goal.motive_case.
				branch_typed_projection
		);
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64) {
			return -1;
		}
		reindexed_binder_count = source_operation_case->binder_count;
		uint32_t branch_projection_substitution;
		int branch_context_status = operation_typed_projection_materialized_context(
			ctx,
			equation->payload.classifier.goal.motive_case.
				branch_typed_projection,
			&motive_case_context,
			&branch_projection_substitution
		);
		if (branch_context_status != 0) {
			return branch_context_status < 0 ? -1 : 1;
		}
		if (motive_case_context == PROTOTYPE_INVALID_ID ||
			branch_projection_substitution == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
			reindexed_binders[i] = source_operation_case->binder_ids[i];
		}
		int refinement_status = operation_motive_case_refinement_substitution(
			ctx, equation, motive_case_context, &case_substitution
		);
		if (refinement_status != 0) {
			return refinement_status < 0 ? -1 : 1;
		}
		branch_refinement_substitutions[case_index] = case_substitution;
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			uint32_t branch_equation = PROTOTYPE_INVALID_ID;
			uint32_t branch_operation =
				equation->payload.classifier.goal.motive_case.branch_operation;
			(void)operation_solver_projected_classifier_equation_id(
				ctx,
				equation->payload.classifier.goal.motive_case.
					branch_typed_projection,
				&branch_equation
			);
			fprintf(stderr,
				"motive-branch operation=%u ast=%u projection=%u case=%u "
				"branch-operation=%u branch-projection=%u branch-equation=%u "
				"branch-ast=%u branch-tag=%d classifier=%u context=%u "
				"projection-action=%u "
				"refinement-action=%u\n",
				typing->operation, operation->source_ast, match_typed_projection,
				case_index, branch_operation,
				equation->payload.classifier.goal.motive_case.
					branch_typed_projection,
				branch_equation,
				ctx->metadata->typed_occurrences.occurrences[
					branch_operation
				].source_ast,
				ctx->metadata->typed_occurrences.occurrences[
					branch_operation
				].tag,
				branch_classifier, motive_case_context,
				branch_projection_substitution, case_substitution);
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
			joined_totality = prototype_classifier_totality_join(
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
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-build failed stage=constructor operation=%u "
					"projection=%u case=%u owner=%u ordinal=%u\n",
					typing->operation, match_typed_projection, case_index,
					source_case->constructor_owner,
					source_case->constructor_id);
			}
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
				) != 0) {
				return -1;
			}
			if (prototype_term_app(
					ctx->terms, constructor_term, field, &constructor_term
				) != 0) {
				return -1;
			}
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					motive_case_context,
					source_operation_case->binder_ids[i],
					&binding_context
				) != 0) {
				if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
					fprintf(stderr,
						"motive-build failed stage=field-context operation=%u "
						"projection=%u case=%u field=%u context=%u binding=%u\n",
						typing->operation, match_typed_projection, case_index, i,
						motive_case_context,
						source_operation_case->binder_ids[i]);
				}
				return -1;
			}
			binding_entry = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				);
			if (!binding_entry) {
				return -1;
			}
			argument_classifiers[i] = operation_context_classifier_term(
				ctx, binding_context
			);
			if (argument_classifiers[i] == PROTOTYPE_INVALID_ID) {
				if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
					fprintf(stderr,
						"motive-build waiting stage=field-classifier operation=%u "
						"projection=%u case=%u field=%u context=%u\n",
						typing->operation, match_typed_projection, case_index, i,
						binding_context);
				}
				return 1;
			}
		}
		int constructor_status = operation_constructor_spine_classifier(
				ctx,
				motive_case_context,
				constructor_term, source_operation_case->constructor_owner,
				argument_classifiers, source_case->binder_count,
				&constructor_classifier, &constructor_saturated
			);
		if (constructor_status != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
			!constructor_saturated) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-build failed stage=constructor-spine operation=%u "
					"projection=%u case=%u status=%d saturated=%d classifier=%u\n",
					typing->operation, match_typed_projection, case_index,
					constructor_status, constructor_saturated,
					constructor_classifier);
			}
			return constructor_status < 0 ? -1 : 1;
		}
		if (prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_term, case_substitution,
				&branch_constructor_terms[case_index]
			) != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-build failed stage=constructor-reindex operation=%u "
					"projection=%u case=%u term=%u substitution=%u\n",
					typing->operation, match_typed_projection, case_index,
					constructor_term, case_substitution);
			}
			return -1;
		}
		if (prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_classifier, case_substitution,
				&branch_constructor_results[case_index]
			) != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-build failed stage=classifier-reindex operation=%u "
					"projection=%u case=%u classifier=%u substitution=%u\n",
					typing->operation, match_typed_projection, case_index,
					constructor_classifier, case_substitution);
			}
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
	uint32_t scrutinee_typed_projection;
	if (compile_occurrence_child(
			ctx, typing->operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0 || operation_solver_projected_child(
			ctx, match_typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_typed_projection
		) != 0) {
		return -1;
	}
	uint32_t scrutinee_classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_typed_projection
	);
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	int has_indexed_scrutinee = scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) == 0 && scrutinee_type < ctx->type_declarations->semantic_schema.type_count;
	uint32_t index_count = has_indexed_scrutinee ?
		ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type].index_count : 0;
	uint32_t scrutinee_parameter_count = has_indexed_scrutinee ?
		ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type].parameter_count : 0;
	/* A motive synthesized by a recursive-use equation becomes
	 * authoritative only after applying it to every constructor equation recovers
	 * that branch's independently synthesized classifier. This handles families
	 * such as Vec append, where the varying index occurs below Pi and cannot be
	 * recovered by comparing only the branch result roots. The zero-index case is
	 * equally important for a quoted non-uniform motive such as BadNat. */
	uint32_t provisional_motive =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		].motive;
	uint32_t motive_lambda_count = 0;
	uint32_t motive_cursor = provisional_motive;
	while (motive_cursor < ctx->terms->term_count &&
		ctx->terms->terms[motive_cursor].tag == PROTOTYPE_TERM_LAMBDA &&
		motive_lambda_count <= index_count) {
		motive_lambda_count++;
		motive_cursor = ctx->terms->terms[motive_cursor].as.lambda.body;
	}
	int validated_indexed_motive =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		].phase != OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE &&
		motive_lambda_count == index_count + 1;
	for (uint32_t case_index = 0;
		validated_indexed_motive && case_index < operation->case_count;
		++case_index) {
		struct operation_motive_equation_validation validation;
		int validation_status = operation_motive_validate_equation(
			ctx,
			typing->operation,
			match_typed_projection,
			case_index,
			provisional_motive,
			&validation
		);
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") ||
			getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(
				stderr,
				"motive-build-validate operation=%u case=%u status=%d motive=%u "
				"expected=%u actual=%u recursive=%d ih=%u guarded=%d conversion=%d\n",
				typing->operation,
				case_index,
				validation_status,
				provisional_motive,
				validation.expected_classifier,
				validation.actual_classifier,
				validation.used_recursive_equation,
				validation.ih_operation,
				validation.guardedness_status,
				validation.conversion_status
			);
		}
		if (validation_status == OPERATION_MOTIVE_EQUATION_INVALID) {
			return -1;
		}
		if (validation_status != OPERATION_MOTIVE_EQUATION_VALID &&
			(validation.used_recursive_equation ||
			 operation_motive_has_recursive_equations(
				ctx, typing->operation, match_typed_projection
			 ) > 0)) {
			/* Recursive candidates are retained until their exact IH projections
			 * and downstream branch classifiers stabilize. A unary fallback here
			 * would erase the equation before it can be checked. */
			return 1;
		}
		validated_indexed_motive =
			validation_status == OPERATION_MOTIVE_EQUATION_VALID;
	}
	if (validated_indexed_motive) {
		uint32_t finalized_motive = provisional_motive;
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(
				stderr,
				"motive-validated operation=%u projection=%u motive=%u "
				"returns-computation=%d effect=%u totality=%d\n",
				typing->operation, match_typed_projection, provisional_motive,
				returns_computation, joined_effect_row, joined_totality
			);
		}
		if (returns_computation) {
			int envelope_status = operation_motive_replace_computation_envelope(
				ctx,
				provisional_motive,
				joined_effect_row,
				joined_totality,
				&finalized_motive
			);
			if (envelope_status < 0) {
				return -1;
			}
			/* A quoted motive may distribute computation classifiers through a
			 * Match instead of carrying one outer computation envelope. Equation
			 * validation above already checks that representation branch-by-branch. */
			if (envelope_status == 0) {
				int close_status = operation_solver_close_unowned_effect_rows(
					ctx,
					typing->operation,
					finalized_motive,
					&finalized_motive
				);
				if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
					fprintf(
						stderr,
						"motive-envelope operation=%u projection=%u status=%d "
						"close=%d finalized=%u\n",
						typing->operation, match_typed_projection,
						envelope_status, close_status, finalized_motive
					);
				}
				if (close_status != 0) {
					return -1;
				}
			}
		}
		uint32_t classifier = finalized_motive;
		for (uint32_t index = 0; index < index_count; ++index) {
			if (prototype_term_app(
					ctx->terms, classifier,
					scrutinee_arguments[scrutinee_parameter_count + index],
					&classifier
				) != 0) {
				if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
					fprintf(
						stderr,
						"motive-apply-index-failed operation=%u projection=%u "
						"index=%u count=%u motive=%u argument=%u terms=%zu\n",
						typing->operation, match_typed_projection, index,
						index_count, classifier,
						scrutinee_arguments[scrutinee_parameter_count + index],
						ctx->terms->term_count
					);
				}
				return -1;
			}
		}
		if (prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			) != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(
					stderr,
					"motive-apply-scrutinee-failed operation=%u projection=%u "
					"index-count=%u motive=%u scrutinee=%u terms=%zu\n",
					typing->operation, match_typed_projection, index_count,
					classifier, match->as.match.scrutinee,
					ctx->terms->term_count
				);
			}
			return -1;
		}
		*p_validated_motive = finalized_motive;
		return 0;
	}
	/* Quoting branch classifiers is a fallback for indexed motives that cannot
	 * be reconstructed from the explicit recursive-use equations. Prefer the
	 * validated index-and-value motive above: a unary quoted Match does not carry
	 * enough information to replay an IH at a recursive field's index. */
	if (has_indexed_scrutinee) {
		int quoted_status = operation_solver_build_quoted_motive(
			ctx,
			typing,
			match_typed_projection,
			operation,
			match,
			branch_results,
			branch_constructor_terms,
			branch_constructor_results,
			branch_refinement_substitutions,
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
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(stderr,
				"motive-path operation=%u projection=%u path=quoted status=%d\n",
				typing->operation, match_typed_projection, quoted_status);
		}
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
				ctx->terms, branch_results[i],
				branch_constructor_terms[i], motive_var, &generalized_branch
			) != 0 || generalized_branch == branch_results[i] ||
			prototype_term_graph_substitute_bound_var(
				ctx->terms, generalized_branch,
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
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(stderr,
				"motive-path operation=%u projection=%u path=ordinary status=0\n",
				typing->operation, match_typed_projection);
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
			if (prototype_type_projection_instance_info(
					ctx->terms,
			&ctx->type_declarations->semantic_schema, branch_results[branch], &branch_result_type,
					branch_result_arguments[branch], &branch_result_count
				) != 0 || prototype_type_projection_instance_info(
					ctx->terms,
			&ctx->type_declarations->semantic_schema, branch_constructor_results[branch], &constructor_type,
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
				prototype_type_projection_instance_make(
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
			int result_status = prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			);
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(stderr,
					"motive-path operation=%u projection=%u "
					"path=result-instance status=%d\n",
					typing->operation, match_typed_projection, result_status);
			}
			return result_status;
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
		if (prototype_type_projection_instance_info(
				ctx->terms,
			&ctx->type_declarations->semantic_schema, branch_results[i], &branch_type,
				result_arguments[i], &result_argument_counts[i]
			) != 0 || prototype_type_projection_instance_info(
				ctx->terms,
			&ctx->type_declarations->semantic_schema, branch_constructor_results[i], &constructor_type,
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
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, branch_constructor_results[0], &(uint32_t){0},
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
		if (can_abstract && (typing_core_normalize_complete(
				ctx->core_requests,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				leaf, &normalized_leaf
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms, normalized_leaf,
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
					ctx->terms, generalized,
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
			if (prototype_type_projection_instance_info(
					ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier, &source_type,
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
			prototype_type_projection_instance_make(
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
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(stderr,
				"motive-path operation=%u projection=%u path=indexed status=0\n",
				typing->operation, match_typed_projection);
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
	int fallback_status = prototype_term_app(
		ctx->terms, classifier, match->as.match.scrutinee, p_classifier
	);
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
		fprintf(stderr,
			"motive-path operation=%u projection=%u path=branch-match status=%d\n",
			typing->operation, match_typed_projection, fallback_status);
	}
	return fallback_status;
}


static int build_operation_uniform_motive(
	struct compile_context* ctx,
	const struct prototype_typing_match_goal* typing,
	uint32_t match_typed_projection,
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
		int impossible = operation_motive_case_is_impossible(
			ctx, operation, case_index
		);
		if (impossible < 0) {
			return -1;
		}
		if (impossible > 0) {
			continue;
		}
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t branch_classifier = operation_solver_classifier_for_projection(
			ctx,
			equation->payload.classifier.goal.motive_case.
				branch_typed_projection
		);
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
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
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
	const struct prototype_typing_match_goal* typing,
	uint32_t match_typed_projection,
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
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
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
		reindexed_binder_count = source_operation_case->binder_count;
		uint32_t branch_projection_substitution;
		int branch_context_status = operation_typed_projection_materialized_context(
			ctx,
			equation->payload.classifier.goal.motive_case.
				branch_typed_projection,
			&motive_case_context,
			&branch_projection_substitution
		);
		if (branch_context_status > 0) {
			motive_case_context = operation_case_materialized_context(
				ctx, operation->first_case + case_index
			);
		} else if (branch_context_status < 0) {
			return -1;
		}
		if (motive_case_context == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		(void)branch_projection_substitution;
		for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
			reindexed_binders[i] = source_operation_case->binder_ids[i];
		}
		int refinement_status = operation_motive_case_refinement_substitution(
			ctx, equation, motive_case_context, &case_substitution
		);
		if (refinement_status > 0 && prototype_substitution_identity(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				motive_case_context,
				&case_substitution
			) != 0) {
			return -1;
		}
		if (refinement_status < 0) {
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
		const struct prototype_typed_occurrence* branch =
			&ctx->metadata->typed_occurrences.occurrences[
				equation->payload.classifier.goal.motive_case.branch_operation
			];
		uint32_t branch_classifier = operation_solver_classifier_for_projection(
			ctx,
			equation->payload.classifier.goal.motive_case.branch_typed_projection
		);
		if (branch->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
			branch->ih_owner_occurrence == typing->operation) {
			uint32_t branch_argument;
			if (compile_occurrence_child(
					ctx, equation->payload.classifier.goal.motive_case.branch_operation,
					PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
					&branch_argument
				) != 0) {
				return -1;
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
					&ctx->terms->case_binders[
						source_case->first_binder + binder_index
					];
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
				) != 0 || prototype_term_induction_hypothesis(
					ctx->terms, motive_frame, recursive_var,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		} else if (branch_classifier != PROTOTYPE_INVALID_ID) {
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
			return 1;
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
	uint32_t scrutinee_typed_projection;
	uint32_t scrutinee_classifier = PROTOTYPE_INVALID_ID;
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count = 0;
	uint32_t parameter_count = 0;
	uint32_t index_count = 0;
	if (compile_occurrence_child(
			ctx, typing->operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) == 0 && operation_solver_projected_child(
			ctx, match_typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_typed_projection
		) == 0) {
		scrutinee_classifier = operation_solver_classifier_for_projection(
			ctx, scrutinee_typed_projection
		);
	}
	if (scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier, &scrutinee_type,
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
				ctx->terms, motive_match,
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
	uint32_t match_typed_projection,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	struct prototype_typing_match_goal typing_storage;
	const struct prototype_typing_match_goal* typing =
		operation_match_typing_view(
			ctx, operation_id, &typing_storage
		) == 0 ? &typing_storage : NULL;
	struct prototype_typing_motive_solution* solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	/* An expected equation is a persistent edge in the typing graph.  Its first
	 * producer may run before the scrutinee classifier is available, so consume
	 * the stored equation here whenever the Match projection is revisited.  This
	 * keeps progress dependent on authoritative graph state rather than on the
	 * order in which a recursive refinement happened to call the helper. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH && typing &&
		solution->phase != OPERATION_MOTIVE_PHASE_MATERIALIZED &&
		solution->expected_equation_classifier < ctx->terms->term_count &&
		prototype_typing_constraint_has_domain(
			&ctx->typing->constraints,
			solution->expected_equation_constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		uint32_t motive_application = PROTOTYPE_INVALID_ID;
		int expected_changed = 0;
		int expected_status = operation_motive_offer_expected_equation(
			ctx,
			operation_id,
			solution->expected_equation_classifier,
			solution->expected_equation_constraint_id,
			&motive_application,
			&expected_changed
		);
		*p_changed |= expected_changed;
		if (expected_status < 0) {
			return -1;
		}
		if (expected_status > 0) {
			if (motive_application >= ctx->terms->term_count) {
				return -1;
			}
			return operation_solver_publish_projection(
				ctx, match_typed_projection, motive_application, p_changed
			);
		}
		solution = &ctx->typing->constraints.
			motive_solution_for_projection[match_typed_projection];
	}
	uint8_t phase = solution->phase;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !typing ||
		phase == OPERATION_MOTIVE_PHASE_NONE ||
		phase == OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE ||
		(phase == OPERATION_MOTIVE_PHASE_MATERIALIZED &&
		 operation_solver_classifier_for_projection(
			ctx, match_typed_projection
		 ) != PROTOTYPE_INVALID_ID)) {
		return 0;
	}
	uint32_t classifier;
	int status;
	uint32_t selected_motive =
		solution->motive;
	uint64_t selected_recursive_revision =
		solution->recursive_equation_revision;
	uint32_t validated_motive = PROTOTYPE_INVALID_ID;
	int selected_validated_motive = 0;
	int recursive_equation_status = operation_motive_has_recursive_equations(
		ctx, operation_id, match_typed_projection
	);
	if (recursive_equation_status < 0) {
		return -1;
	}
	if (phase == OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE) {
		status = build_operation_motive(
			ctx, typing, match_typed_projection, &classifier,
			&validated_motive
		);
		if (status == 0) {
			selected_validated_motive =
				validated_motive != PROTOTYPE_INVALID_ID;
		} else if (status > 0) {
			status = build_operation_guarded_recursive_motive(
				ctx, typing, match_typed_projection, &classifier
			);
		}
	} else if (selected_motive < ctx->terms->term_count) {
		/* A projection-local motive candidate is an equation, not a hint for a
		 * second synthesis route. Validate that exact candidate against every branch
		 * before deriving anything from branch classifiers. Rebuilding a uniform
		 * motive first reverses this dependency and erases ordinary dependent motives
		 * as well as indexed or guarded recursive ones. */
		status = build_operation_motive(
			ctx, typing, match_typed_projection, &classifier,
			&validated_motive
		);
		selected_validated_motive = status == 0 &&
			validated_motive != PROTOTYPE_INVALID_ID;
		if (status > 0 && recursive_equation_status == 0) {
			status = build_operation_uniform_motive(
				ctx, typing, match_typed_projection, &classifier
			);
			if (status > 0) {
				status = build_operation_motive(
					ctx, typing, match_typed_projection, &classifier,
					&validated_motive
				);
			}
		}
	} else {
		status = build_operation_uniform_motive(
			ctx, typing, match_typed_projection, &classifier
		);
		if (status > 0) {
			status = build_operation_motive(
				ctx, typing, match_typed_projection, &classifier,
				&validated_motive
			);
		}
	}
	if (status != 0) {
		if (status < 0) {
			fprintf(
				stderr,
				"motive materialization invalid operation=%u phase=%u "
				"selected=%u recursive=%d revision=%" PRIu64 "\n",
				operation_id,
				phase,
				selected_motive,
				recursive_equation_status,
				selected_recursive_revision
			);
			for (uint32_t case_index = 0;
				case_index < operation->case_count;
				++case_index) {
				const struct prototype_typing_constraint* equation =
					operation_solver_motive_equation_for_projection(
						ctx, match_typed_projection, case_index
					);
				fprintf(
					stderr,
					"motive equation operation=%u case=%u constraint=%u "
					"state=%d result=%u owner-projection=%u\n",
					operation_id,
					case_index,
					equation ? equation->id : PROTOTYPE_INVALID_ID,
					equation ? ctx->typing->constraints.solutions[
						equation->id
					].state : -1,
					equation ? ctx->typing->constraints.solutions[
						equation->id
					].result_term : PROTOTYPE_INVALID_ID,
					equation ? equation->owner_typed_projection :
						PROTOTYPE_INVALID_ID
				);
			}
		}
		return status < 0 ? -1 : 0;
	}
	/* Motive construction may solve an ordinary dependency which activates a
	 * previously generated IH edge for this same owner. Do not publish a result
	 * built from the older equation set; this is an owner-local transaction epoch,
	 * not validation-time discovery or a solver-global invalidation counter. */
	if (solution->recursive_equation_revision !=
			selected_recursive_revision &&
		solution->motive != selected_motive) {
		return 0;
	}
	/* The constant candidate is a bootstrap equation from one non-recursive
	 * branch, not a proposed classifier for the complete Match.  In a dependent
	 * motive the complete classifier need not be convertible to that branch
	 * equation; build_operation_motive has already checked every equation. */
	uint32_t motive = selected_validated_motive ?
		(validated_motive != PROTOTYPE_INVALID_ID ?
			validated_motive : selected_motive) : PROTOTYPE_INVALID_ID;
	if (!selected_validated_motive) {
		if (classifier >= ctx->terms->term_count ||
			ctx->terms->terms[classifier].tag != PROTOTYPE_TERM_APP) {
			return -1;
		}
		motive = ctx->terms->terms[classifier].as.app.function;
		while (motive < ctx->terms->term_count &&
			ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
			motive = ctx->terms->terms[motive].as.app.function;
		}
	}
	if (motive >= ctx->terms->term_count ||
		ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
		fprintf(
			stderr,
			"motive-publish operation=%u projection=%u phase=%u classifier=%u "
			"motive=%u selected=%u recursive=%d selected-revision=%" PRIu64 " "
			"current-revision=%" PRIu64 "\n",
			operation_id, match_typed_projection, phase, classifier, motive,
			selected_motive, selected_validated_motive,
			selected_recursive_revision, solution->recursive_equation_revision
		);
	}
	int motive_changed = 0;
	if (phase != OPERATION_MOTIVE_PHASE_MATERIALIZED) {
		struct prototype_typing_motive_solution next =
			*solution;
		next.motive = motive;
		next.status = OPERATION_MOTIVE_STATUS_SOLVED;
		next.phase = OPERATION_MOTIVE_PHASE_MATERIALIZED;
		int transition_status = operation_motive_solution_transition_for_projection(
				ctx, match_typed_projection, &next,
				OPERATION_MOTIVE_TRANSITION_MATERIALIZATION, 0, &motive_changed
			);
		if (transition_status != 0) {
			if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
				fprintf(
					stderr,
					"motive-publish-transition-failed operation=%u projection=%u "
					"status=%d\n",
					operation_id, match_typed_projection, transition_status
				);
			}
			return -1;
		}
		if (motive_changed && p_changed) {
			*p_changed = 1;
		}
	}
	uint32_t current_classifier = operation_solver_classifier_for_projection(
		ctx, match_typed_projection
	);
	int bind_status;
	if (current_classifier == PROTOTYPE_INVALID_ID) {
		bind_status = operation_solver_publish_projection(
			ctx, match_typed_projection, classifier, p_changed
		);
	} else if (prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			current_classifier,
			classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		bind_status = 0;
	} else {
		/* Branch candidates are bootstrap equations. Once motive construction has
		 * consumed the complete branch equation set, materialization is the unique
		 * refinement authority for both recursive and non-recursive Matches. */
		bind_status = operation_solver_publish_projection(
			ctx, match_typed_projection, classifier, p_changed
		);
	}
	if (bind_status != 0) {
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(
				stderr,
				"motive-publish-bind-failed operation=%u projection=%u status=%d\n",
				operation_id, match_typed_projection, bind_status
			);
		}
		return bind_status;
	}
	/* Motive materialization is solver state that IH constraints consume. The
	 * Match classifier may already have been provisionally bound to the same
	 * Term, so classifier binding alone is not guaranteed to wake them. Wake
	 * exactly once for the materialization transition, not on every fixed-point
	 * visit that observes the already materialized motive. */
	if (!motive_changed) {
		return 0;
	}
	int enqueue_status = operation_solver_enqueue_projection_dependents(
		ctx, match_typed_projection
	);
	if (enqueue_status != 0 && getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
		fprintf(
			stderr,
			"motive-publish-enqueue-failed operation=%u projection=%u status=%d\n",
			operation_id, match_typed_projection, enqueue_status
		);
	}
	return enqueue_status;
}

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		if (!projection || projection->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->metadata->typed_occurrences.occurrences[
				projection->source_occurrence
			].tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			continue;
		}
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			const struct prototype_typing_motive_solution* trace_solution =
				&ctx->typing->constraints.motive_solution_for_projection[
					typed_projection
				];
			fprintf(
				stderr,
				"motive-materialize projection=%u operation=%u status=%u "
				"phase=%u motive=%u candidate=%u\n",
				typed_projection, projection->source_occurrence,
				trace_solution->status, trace_solution->phase,
				trace_solution->motive, trace_solution->constant_candidate
			);
			if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL &&
				trace_solution->motive < ctx->terms->term_count) {
				fprintf(stderr, "motive-term operation=%u projection=%u ",
					projection->source_occurrence, typed_projection);
				prototype_type_term_print_debug(
					stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
					trace_solution->motive
				);
				fprintf(stderr, "\n");
			}
		}
		if (operation_solver_materialize_match_solution(
				ctx, projection->source_occurrence, typed_projection, p_changed
			) != 0) {
			fprintf(stderr,
				"classifier solver motive materialization failed "
				"operation=%u projection=%u\n",
				projection->source_occurrence, typed_projection);
			return -1;
		}
	}
	return 0;
}

static int operation_motive_refresh_terminal_status(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	struct prototype_typing_motive_solution next =
		ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (next.status == OPERATION_MOTIVE_STATUS_SOLVED) {
		return 0;
	}
	int contradiction = 0;
	int residual = 0;
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[operation];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation) {
			return -1;
		}
		const struct prototype_typing_constraint_solution* equation_solution =
			&ctx->typing->constraints.solutions[equation->id];
		if (equation_solution->state ==
			OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			contradiction = 1;
		}
		if (equation_solution->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			residual = 1;
		}
		if (equation_solution->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			residual = 1;
		}
	}
	if (prototype_typing_constraint_has_domain(
			&ctx->typing->constraints,
			next.expected_equation_constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		const struct prototype_typing_constraint* expected =
			&ctx->typing->constraints.constraints[next.expected_equation_constraint_id];
		const struct prototype_typing_constraint_solution* expected_solution =
			&ctx->typing->constraints.solutions[expected->id];
		if (expected_solution->state ==
			OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			contradiction = 1;
		}
		if (expected_solution->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			residual = 1;
		}
		if (expected_solution->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			residual = 1;
		}
	}
	if (contradiction) {
		next.status = OPERATION_MOTIVE_STATUS_CONTRADICTION;
		return operation_motive_solution_transition_for_projection(
			ctx, match_typed_projection, &next,
			OPERATION_MOTIVE_TRANSITION_CONTRADICTION, 0, NULL
		);
	}
	if (residual) {
		next.status = OPERATION_MOTIVE_STATUS_RESIDUAL;
		return operation_motive_solution_transition_for_projection(
			ctx, match_typed_projection, &next,
			OPERATION_MOTIVE_TRANSITION_RESIDUAL,
			0, NULL
		);
	}
	next.status = OPERATION_MOTIVE_STATUS_WAITING;
	return operation_motive_solution_transition_for_projection(
		ctx, match_typed_projection, &next,
		OPERATION_MOTIVE_TRANSITION_DEPENDENCY, 0, NULL
	);
}

static void operation_motive_print_pending_classifier_chain(
	const struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t depth
) {
	if (!ctx || !ctx->metadata || !ctx->typing || depth > 12 ||
		typed_projection >= ctx->typing->context_projections.typed_projection_count) {
		return;
	}
	uint32_t equation_id = PROTOTYPE_INVALID_ID;
	if (operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &equation_id
		) != 0 || !prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, equation_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return;
	}
	const struct prototype_typing_constraint* equation =
		&ctx->typing->constraints.constraints[equation_id];
	const struct prototype_typing_constraint_solution* solution =
		&ctx->typing->constraints.solutions[equation_id];
	const struct prototype_typed_occurrence* occurrence = equation->source_occurrence <
		ctx->metadata->typed_occurrences.occurrence_count ?
		&ctx->metadata->typed_occurrences.occurrences[equation->source_occurrence] : NULL;
	fprintf(
		stderr,
		"motive solver: dependency depth=%u projection=%u equation=%u kind=%d "
		"source=%u tag=%d context=%u binding=%u operands=%u state=%d reason=%d "
		"answer=%d result=%u\n",
		depth, typed_projection, equation_id, equation->kind,
		equation->source_occurrence, occurrence ? occurrence->tag : -1,
		occurrence ? occurrence->context_id : PROTOTYPE_INVALID_ID,
		occurrence ? occurrence->binding_id : PROTOTYPE_INVALID_ID,
		equation->operand_count, solution->state, solution->reason,
		solution->answer_state, solution->result_term
	);
	for (uint32_t operand = 0;
		operand < equation->operand_count && operand < 4;
		++operand) {
		uint32_t child = operation_solver_constraint_operand_projection(ctx, equation, operand);
		if (child == typed_projection) {
			fprintf(
				stderr,
				"motive solver: dependency cycle projection=%u operand=%u\n",
				typed_projection, operand
			);
			continue;
		}
		operation_motive_print_pending_classifier_chain(ctx, child, depth + 1);
	}
}

/* Close the Layer T Match projection before semantic solutions are sealed.
 * Every accepted Match must already have a solver-owned motive; this stage may
 * canonicalize that type expression, but it must not reconstruct a missing
 * motive from the Match classifier. */
static int operation_solver_close_match_motive_projections(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* graph =
		&ctx->metadata->typed_occurrences;
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		if (!projection ||
			projection->source_occurrence >= graph->occurrence_count) {
			continue;
		}
		uint32_t operation = projection->source_occurrence;
		struct prototype_typed_occurrence* occurrence = &graph->occurrences[operation];
		if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			continue;
		}
		uint32_t classifier_equation;
		if (operation_solver_projected_classifier_equation_id(
				ctx, typed_projection, &classifier_equation
			) != 0) {
			continue;
		}
		struct prototype_typing_motive_solution* solution =
			&ctx->typing->constraints.motive_solution_for_projection[
				typed_projection
			];
		if (operation_motive_refresh_terminal_status(
				ctx, operation, typed_projection
			) != 0) {
			return -1;
		}
		if (solution->status != OPERATION_MOTIVE_STATUS_SOLVED ||
			solution->phase != OPERATION_MOTIVE_PHASE_MATERIALIZED ||
			solution->motive >= ctx->terms->term_count) {
			fprintf(
				stderr,
				"motive solver: unresolved Match before solution seal "
				"operation=%u projection=%u status=%u phase=%u motive=%u "
				"expected=%u expected-equation=%u expected-context=%u\n",
				operation,
				typed_projection,
				solution->status,
				solution->phase,
				solution->motive,
				solution->expected_equation_classifier,
				solution->expected_equation_constraint_id,
				solution->expected_equation_context_id
			);
			for (uint32_t edge_id = 0;
				edge_id < ctx->typing->context_projections.typed_projection_edge_count;
				++edge_id) {
				const struct prototype_typed_projection_edge* edge =
					prototype_typed_projection_edge_get(
						&ctx->typing->context_projections, edge_id
					);
				if (edge && edge->child_typed_projection == typed_projection) {
					fprintf(
						stderr,
						"motive solver: incoming edge=%u kind=%d parent=%u "
						"relation=%u\n",
						edge_id, edge->kind, edge->parent_typed_projection,
						edge->source_relation
					);
				}
			}
			for (uint32_t constraint_id = 0;
				constraint_id < ctx->typing->constraints.constraint_count;
				++constraint_id) {
				const struct prototype_typing_constraint* related =
					&ctx->typing->constraints.constraints[constraint_id];
				int mentions_projection =
					related->owner_typed_projection == typed_projection;
				for (uint32_t operand = 0;
					!mentions_projection && operand < related->operand_count;
					++operand) {
					mentions_projection = operation_solver_constraint_operand_projection(ctx, related, operand) ==
						typed_projection;
				}
				if (mentions_projection) {
					const struct prototype_typing_constraint_solution* related_solution =
						&ctx->typing->constraints.solutions[constraint_id];
					fprintf(
						stderr,
						"motive solver: related equation=%u domain=%d kind=%d "
						"owner=%u source=%u operands=%u state=%d reason=%d result=%u\n",
						constraint_id, related->domain, related->kind,
						related->owner_typed_projection, related->source_occurrence,
						related->operand_count, related_solution->state,
						related_solution->reason, related_solution->result_term
					);
					if (related->kind == OPERATION_CONSTRAINT_PI_EXPECTED) {
						fprintf(
							stderr,
							"motive solver: related Pi role=%d function-operation=%u "
							"argument-operation=%u",
							related->payload.classifier.goal.pi.role,
							related->payload.classifier.goal.pi.body_or_function_operation,
							related->payload.classifier.goal.pi.
								domain_classifier_or_argument_occurrence
						);
						for (uint32_t operand = 0;
							operand < related->operand_count;
							++operand) {
							fprintf(
								stderr,
								" operand[%u]=%u",
								operand,
								operation_solver_constraint_operand_projection(ctx, related, operand)
							);
						}
						fputc('\n', stderr);
					}
				}
			}
			for (uint32_t case_index = 0;
				case_index < occurrence->case_count;
				++case_index) {
				const struct prototype_typing_constraint* equation =
					operation_solver_motive_equation_for_projection(
						ctx, typed_projection, case_index
					);
				uint32_t branch_projection = PROTOTYPE_INVALID_ID;
				if (equation) {
					branch_projection = equation->payload.classifier.goal.
						motive_case.branch_typed_projection;
				}
				uint32_t branch_classifier = PROTOTYPE_INVALID_ID;
				uint32_t branch_classifier_equation = PROTOTYPE_INVALID_ID;
				if (branch_projection < ctx->typing->context_projections.
					typed_projection_count) {
					branch_classifier = operation_solver_classifier_for_projection(
						ctx, branch_projection
					);
					(void)operation_solver_projected_classifier_equation_id(
						ctx, branch_projection, &branch_classifier_equation
					);
				}
				fprintf(
					stderr,
					"motive solver: unresolved branch operation=%u projection=%u "
					"case=%u branch-projection=%u classifier=%u "
					"constraint-state=%d\n",
					operation,
					typed_projection,
					case_index,
					branch_projection,
					branch_classifier,
					equation ? ctx->typing->constraints.solutions[equation->id].state : -1
				);
				if (prototype_typing_constraint_has_domain(
						&ctx->typing->constraints, branch_classifier_equation,
						OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
					)) {
					const struct prototype_typing_constraint* branch_equation =
						&ctx->typing->constraints.constraints[branch_classifier_equation];
					const struct prototype_typing_constraint_solution* branch_solution =
						&ctx->typing->constraints.solutions[branch_classifier_equation];
					fprintf(
						stderr,
						"motive solver: branch classifier equation=%u kind=%d source=%u "
						"operands=%u state=%d reason=%d answer=%d result=%u "
						"producer=%u revision=%llu input-revision=%llu "
						"context-revision=%llu\n",
						branch_classifier_equation, branch_equation->kind,
						branch_equation->source_occurrence, branch_equation->operand_count,
						branch_solution->state, branch_solution->reason,
						branch_solution->answer_state, branch_solution->result_term,
						branch_solution->producer_constraint_id,
						(unsigned long long)branch_solution->answer_revision,
						(unsigned long long)
							branch_solution->input_revision,
						(unsigned long long)
							branch_solution->answer_context_revision
					);
					for (uint32_t operand = 0;
						operand < branch_equation->operand_count && operand < 4;
						++operand) {
						uint32_t operand_projection =
							operation_solver_constraint_operand_projection(ctx, branch_equation, operand);
						uint32_t operand_classifier =
							operation_solver_classifier_for_projection(
								ctx, operand_projection
							);
						uint32_t operand_equation = PROTOTYPE_INVALID_ID;
						(void)operation_solver_projected_classifier_equation_id(
							ctx, operand_projection, &operand_equation
						);
						int operand_equation_valid =
							prototype_typing_constraint_has_domain(
								&ctx->typing->constraints, operand_equation,
								OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
							);
						fprintf(
							stderr,
							"motive solver: branch operand=%u projection=%u classifier=%u "
							"equation=%u state=%d reason=%d result=%u\n",
							operand, operand_projection, operand_classifier,
							operand_equation,
							operand_equation_valid ?
								ctx->typing->constraints.solutions[operand_equation].state : -1,
							operand_equation_valid ?
								ctx->typing->constraints.solutions[operand_equation].reason : -1,
							operand_equation_valid ?
								ctx->typing->constraints.solutions[operand_equation].result_term :
								PROTOTYPE_INVALID_ID
						);
					}
					operation_motive_print_pending_classifier_chain(
						ctx, branch_projection, 0
					);
				}
			}
			return -1;
		}
		uint32_t projected_motive;
		if (prototype_type_projection_classifier_graph(
				ctx->terms,
				ctx->type_declarations,
				solution->motive,
				&projected_motive
			) != 0 || projected_motive >= ctx->terms->term_count) {
			return -1;
		}
		struct prototype_typing_motive_solution projected = *solution;
		projected.motive = projected_motive;
		if (operation_motive_solution_transition_for_projection(
				ctx, typed_projection, &projected,
				OPERATION_MOTIVE_TRANSITION_PROJECTION, 0, NULL
			) != 0) {
			return -1;
		}
		/* Every reindexed motive remains authoritative in its projection cell. The
		 * occurrence-level slot is the readback of the one publication projection
		 * selected when the Layer T graph is built. */
		if (typed_projection !=
			ctx->typing->publication_typed_projection_for_occurrence[operation]) {
			continue;
		}
		if (occurrence->match_motive == PROTOTYPE_INVALID_ID) {
			occurrence->match_motive = projected_motive;
			continue;
		}
		struct prototype_term_conversion_result publication_conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				occurrence->match_motive,
				projected_motive
			);
		if (publication_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			fprintf(
				stderr,
				"motive publication conflict operation=%u projection=%u "
				"published=%u projected=%u\n",
				operation, typed_projection, occurrence->match_motive,
				projected_motive
			);
			return -1;
		}
	}
	return 0;
}

/* An IH occurrence is a recursive Match computation. Higher-order recursive
 * fields lift that computation under a Pi and therefore produce a suspended
 * function value. Reintroduce the outer pure computation boundary here so the
 * occurrence classifier agrees with its lowering category and a sequence
 * result has an exact computation producer. */
static int operation_solver_ih_computation_classifier(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t motive,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->terms->terms[classifier].tag ==
		PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		/* A quoted recursive motive uses IH(rest) as its recursive classifier
		 * equation. It is not a materialized value result and must remain under
		 * that motive rather than acquiring a second computation envelope. */
		*p_classifier = classifier;
		return 0;
	}
	uint32_t motive_body = motive;
	while (motive_body < ctx->terms->term_count &&
		ctx->terms->terms[motive_body].tag == PROTOTYPE_TERM_LAMBDA) {
		motive_body = ctx->terms->terms[motive_body].as.lambda.body;
	}
	if (motive_body < ctx->terms->term_count &&
		ctx->terms->terms[motive_body].tag == PROTOTYPE_TERM_MATCH) {
		/* A quoted non-uniform motive is already a classifier computation:
		 * M(rest) remains neutral until rest is known. Wrapping that neutral Match
		 * would change Comp(E, M(rest)) into a different classifier. */
		*p_classifier = classifier;
		return 0;
	}
	struct prototype_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			classifier,
			&view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION) {
		*p_classifier = classifier;
		return 0;
	}
	if (view.category != PROTOTYPE_CLASSIFIER_CATEGORY_VALUE) {
		return -1;
	}
	uint32_t empty_effects;
	return prototype_term_effect_row_empty(
			ctx->terms, &empty_effects
		) != 0 || prototype_term_total_computation_type(
			ctx->terms, empty_effects, classifier, p_classifier
		) != 0 ? -1 : 0;
}

/* Keep IH propagation entirely in the solver until its parent motive has
 * been materialized from a solved equation. */
static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t typed_projection,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_projection* projected =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projected || projected->source_occurrence != operation_id) {
		return -1;
	}
	const struct prototype_typing_constraint* ih_equation =
		operation_motive_ih_equation_for_projection_const(
			ctx, typed_projection
		);
	if (!ih_equation) {
		return -1;
	}
	if (ih_equation->operand_count < 2) {
		return -1;
	}
	uint32_t parent_match_typed_projection =
		operation_solver_constraint_operand_projection(ctx, ih_equation, 1);
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	/* source_classifier plus context_action_substitution is the immutable
	 * constructor-telescope premise. The occurrence meta is reserved for the
	 * selected motive applied to the recursive argument; never seed that final
	 * solution cell with the premise. */
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
	if (operation_solver_solve_match(
			ctx, parent_match, parent_match_typed_projection, p_changed
		) != 0) {
		fprintf(stderr, "IH materialization: parent match solve failed operation=%u match=%u\n",
			operation_id, parent_match);
		return -1;
	}
	struct prototype_typing_motive_solution* parent_solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			parent_match_typed_projection
		];
	uint32_t motive = parent_solution->motive;
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
		const struct prototype_typing_constraint_solution* ih_solution =
			&ctx->typing->constraints.solutions[ih_equation->id];
		fprintf(
			stderr,
			"ih-materialize operation=%u projection=%u "
			"motive=%u projected=%u "
			"classifier=%u parent-phase=%u parent-revision=%" PRIu64 "\n",
			operation_id,
			typed_projection,
			motive,
			ih_solution->projected_motive,
			operation_solver_classifier_for_projection(ctx, typed_projection),
			parent_solution->phase,
			parent_solution->recursive_equation_revision
		);
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
		int application_changed = 0;
		if (operation_solver_set_ih_motive_application(
				ctx, operation_id, parent_match, argument_occurrence,
				&application_changed
			) != 0) {
			fprintf(stderr,
				"IH materialization: motive application invalid operation=%u parent=%u "
				"argument=%u\n",
				operation_id, parent_match, argument_occurrence);
			return -1;
		}
		if (application_changed && operation_solver_enqueue_projection_dependents(
				ctx, typed_projection
			) != 0) {
			return -1;
		}
		*p_changed |= application_changed;
		/* A constant motive equation M(_) == T is already a concrete solver
		 * solution for this IH occurrence.  Bind it now so CBPV wrappers can
		 * propagate through RETURN/THUNK/COMPUTATION_FOLD; the lambda for M is still
		 * materialized only after every Match equation has been checked. */
		uint32_t candidate = parent_solution->constant_candidate;
		if (candidate != PROTOTYPE_INVALID_ID) {
			if (operation_solver_ih_computation_classifier(
					ctx, candidate, PROTOTYPE_INVALID_ID, &candidate
				) != 0) {
				return -1;
			}
			uint32_t contextual_candidate;
			int context_status = operation_solver_term_in_projection_context(
				ctx, typed_projection, candidate, &contextual_candidate
			);
			if (context_status < 0) {
				return -1;
			}
			if (context_status > 0) {
				return 0;
			}
			candidate = contextual_candidate;
			uint32_t equation_id;
			if (operation_solver_projected_classifier_equation_id(
					ctx, typed_projection, &equation_id
				) != 0) {
				return -1;
			}
			if (ctx->typing->constraints.solutions[equation_id].result_term ==
					PROTOTYPE_INVALID_ID) {
				/* A constant candidate may seed a virgin cell, but it must not
				 * overwrite an invalidated projection from a stronger motive equation. */
				ctx->typing->constraints.solutions[equation_id].projected_motive =
					PROTOTYPE_INVALID_ID;
				return operation_solver_publish_projection(
					ctx, typed_projection, candidate, p_changed
				);
			}
			return 0;
		}
		return 0;
	}
	if (operation_motive_ih_projection_is_current(
			ctx, typed_projection, motive
		)) {
		/* A more precise recursive equation changes the selected motive before it
		 * reaches this point. For one fixed motive and projected argument, retain
		 * the already materialized IH classifier. */
		return 0;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[parent_match];
	uint32_t match_scrutinee_typed_projection;
	uint32_t scrutinee_classifier = operation_solver_projected_child(
		ctx,
		parent_match_typed_projection,
		PROTOTYPE_TERM_CHILD_SCRUTINEE,
		0,
		&match_scrutinee_typed_projection
	) == 0 ? operation_solver_classifier_for_projection(
		ctx, match_scrutinee_typed_projection
	) : PROTOTYPE_INVALID_ID;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t field_classifier = PROTOTYPE_INVALID_ID;
	uint32_t field_context;
	uint32_t field_action;
	int field_context_status = operation_typed_projection_materialized_context(
		ctx, typed_projection, &field_context, &field_action
	);
	if (field_context_status < 0) {
		return -1;
	}
	if (field_context_status > 0) {
		return 0;
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
			uint32_t projected_owner = scrutinee_classifier;
			struct prototype_context_classifier_view context_view =
				operation_type_context_view(ctx);
			if (operation->ih_field_index < match_case->binder_count &&
				prototype_judgement_constructor_field_classifier_in_view(
					ctx->terms,
					ctx->type_declarations,
					&context_view,
					&ctx->metadata->substitutions,
					field_context,
					projected_owner,
					match_case->constructor_id,
					&ctx->terms->case_binders[match_case->first_binder],
					operation->ih_field_index,
					operation->ih_field_index,
					&field_classifier
				) != 0) {
				fprintf(stderr,
					"IH materialization: field classifier failed operation=%u parent=%u "
					"case=%u field=%u context=%u projected-context=%u projected-action=%u "
					"owner=%u constructor=%u\n",
					operation_id, parent_match, operation->ih_case_index,
					operation->ih_field_index, operation->context_id,
					field_context, field_action,
					projected_owner,
					match_case->constructor_id);
				return -1;
			}
		}
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
		if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
			fprintf(stderr, "IH materialization field-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				field_classifier
			);
			fprintf(stderr, "\nIH materialization scrutinee-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				scrutinee_classifier
			);
			fprintf(stderr, "\nIH materialization motive-term ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms, motive
			);
			fputc('\n', stderr);
		}
		return -1;
	}
	int motive_effort = compile_effort_charge(
		ctx->metadata, ctx->effort_atomic, PROTOTYPE_EFFORT_PHASE_MOTIVE, 1
	);
	if (motive_effort != 0) return motive_effort;
	for (uint32_t reduction = 0; reduction < 4; ++reduction) {
		struct prototype_core_normalization_request request = {
			.term_id = classifier,
			.profile = PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			.step_limit = compile_effort_remaining(ctx->metadata)
		};
		struct prototype_core_normalization_response normalized_classifier;
		if (prototype_core_request_normalize(
				ctx->core_requests, &request, &normalized_classifier
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
	if (operation_solver_ih_computation_classifier(
			ctx, classifier, motive, &classifier
		) != 0) {
		return -1;
	}
	uint32_t contextual_classifier;
	int context_status = operation_solver_term_in_projection_context(
		ctx, typed_projection, classifier, &contextual_classifier
	);
	if (context_status < 0) {
		return -1;
	}
	if (context_status > 0) {
		return 0;
	}
	classifier = contextual_classifier;
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE")) {
		fprintf(
			stderr,
			"ih-derived operation=%u projection=%u argument=%u field=%u "
			"motive=%u classifier=%u\n",
			operation_id,
			typed_projection,
			ctx->metadata->typed_occurrences.occurrences[
				argument_occurrence
			].core_term,
			field_classifier,
			motive,
			classifier
		);
	}
	uint32_t previous = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
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
		return operation_motive_publish_ih_projection(
			ctx, typed_projection, motive, p_changed
		);
	}
	if (previous != PROTOTYPE_INVALID_ID) {
		struct prototype_classifier_view previous_view;
		struct prototype_classifier_view lifted_view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, previous, &previous_view
			) == 0 && prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, classifier, &lifted_view
			) == 0 && previous_view.category ==
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION && lifted_view.category ==
				PROTOTYPE_CLASSIFIER_CATEGORY_VALUE && prototype_term_computation_type(
				ctx->terms, previous_view.effect_row, classifier,
				previous_view.totality, &classifier
			) != 0) {
			return -1;
		}
	}
	if (previous == PROTOTYPE_INVALID_ID) {
		int bind_status = operation_solver_publish_projection(
			ctx, typed_projection, classifier, p_changed
		);
		if (bind_status == 0) {
			if (operation_motive_publish_ih_projection(
					ctx, typed_projection, motive, p_changed
				) != 0) {
				return -1;
			}
		}
		return bind_status;
	}
	int classifiers_equal = prototype_judgement_classifier_conversion(
		ctx->terms, ctx->type_declarations, previous, classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	uint32_t equation_id;
	if (operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &equation_id
		) != 0) {
		return -1;
	}
	const struct prototype_typing_constraint_solution* answer =
		&ctx->typing->constraints.solutions[equation_id];
	const struct prototype_typing_constraint_solution* ih_answer =
		&ctx->typing->constraints.solutions[ih_equation->id];
	int replace_provisional =
		(answer->answer_state ==
			PROTOTYPE_TYPING_EQUATION_ANSWER_PROVISIONAL &&
		 ih_answer->projected_motive == PROTOTYPE_INVALID_ID) ||
		parent_solution->phase == OPERATION_MOTIVE_PHASE_EQUATION_SET ||
		parent_solution->phase == OPERATION_MOTIVE_PHASE_MATERIALIZED;
	if (!classifiers_equal && replace_provisional) {
		/* The recursive equation marks the earlier classifier as a provisional
		 * projection. Its polarity is irrelevant: the materialized owner motive is
		 * authoritative for value, Pi, and computation results alike. */
		if (operation_solver_publish_projection(
				ctx, typed_projection, classifier, p_changed
			) != 0) {
			return -1;
		}
		if (operation_motive_publish_ih_projection(
				ctx, typed_projection, motive, p_changed
			) != 0) {
			return -1;
		}
		return 0;
	}
	if (!classifiers_equal) {
		/* A unary motive inferred before an IH consumer contributes its indexed
		 * equation is provisional. The constructor telescope projection remains
		 * the only typed classifier available at this fixed-point stage. Keep it
		 * until expected refinement records the recursive equation; accepted
		 * materialization will then either replace it from that equation or reject
		 * the jointly validated motive. */
		return 0;
	}
	int widen_status = operation_solver_widen_projection(
		ctx, typed_projection, classifier, p_changed
	);
	if (widen_status != 0) {
		fprintf(stderr,
			"IH materialization: computation widening failed operation=%u previous=%u "
			"classifier=%u status=%d\n",
			operation_id, previous, classifier, widen_status);
	} else {
		if (operation_motive_publish_ih_projection(
				ctx, typed_projection, motive, p_changed
			) != 0) {
			return -1;
		}
	}
	return widen_status;
}
