static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
);

static int operation_effect_row_binder_is_owned(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t binding_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 64; ++depth) {
		const struct prototype_typed_occurrence* wrapper =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t inner = PROTOTYPE_INVALID_ID;
		if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			inner = wrapper->wrapped_occurrence;
		} else if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			inner = wrapper->wrapped_occurrence;
		} else {
			break;
		}
		if (inner >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		operation_id = inner;
	}
	for (uint32_t owner = 0; owner < ctx->metadata->typed_occurrences.occurrence_count; ++owner) {
		const struct prototype_typed_occurrence* lambda =
			&ctx->metadata->typed_occurrences.occurrences[owner];
		if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			continue;
		}
		int declares_binder = 0;
		for (uint32_t row = 0; row < lambda->implicit_effect_row_count; ++row) {
			if (lambda->implicit_effect_row_binders[row] == binding_id) {
				declares_binder = 1;
				break;
			}
		}
		if (!declares_binder) {
			continue;
		}
		uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, owner, operation_id, visited
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
		memset(visited, 0, sizeof(visited));
		contains = operation_subtree_contains_operation(
			ctx, operation_id, owner, visited
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
	}
	return 0;
}

static int operation_solver_close_unowned_effect_rows(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_closed
) {
	if (!ctx || !p_closed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t closed = classifier;
	const uint32_t* variable_terms;
	size_t variable_count;
	if (prototype_term_effect_row_variable_terms(
			ctx->terms, &variable_terms, &variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < variable_count; ++i) {
		uint32_t binding_id = ctx->terms->terms[
			variable_terms[i]
		].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(
				ctx->terms, closed, binding_id
			)) {
			continue;
		}
		int owned = operation_effect_row_binder_is_owned(
			ctx, operation_id, binding_id
		);
		if (owned < 0) {
			return -1;
		}
		if (owned) {
			continue;
		}
		uint32_t empty_row;
		if (prototype_term_effect_row_empty(ctx->terms, &empty_row) != 0 ||
			prototype_term_graph_substitute_bound_var(
				ctx->terms, closed,
				binding_id,
				empty_row,
				&closed
			) != 0) {
			return -1;
		}
	}
	*p_closed = closed;
	return 0;
}

static int operation_classifier_contains_unowned_effect_row(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier
) {
	if (!ctx || !ctx->terms || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const uint32_t* variable_terms;
	size_t variable_count;
	if (prototype_term_effect_row_variable_terms(
			ctx->terms, &variable_terms, &variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < variable_count; ++i) {
		uint32_t binding_id = ctx->terms->terms[
			variable_terms[i]
		].as.effect_row_var.binding_id;
		if (prototype_term_contains_free_binding(
				ctx->terms,
				classifier,
			binding_id
		)) {
			int owned = operation_effect_row_binder_is_owned(
				ctx, operation_id, binding_id
			);
			if (owned < 0) {
				return -1;
			}
			if (!owned) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_effect_constraint_add(
	struct compile_context* ctx,
	int kind,
	uint32_t operation,
	uint32_t result_row,
	uint32_t left_row,
	uint32_t right_row,
	uint32_t origin_constraint_id
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		result_row >= ctx->terms->term_count ||
		left_row >= ctx->terms->term_count ||
		(right_row != PROTOTYPE_INVALID_ID &&
			right_row >= ctx->terms->term_count) ||
		(origin_constraint_id != PROTOTYPE_INVALID_ID &&
			(origin_constraint_id >= ctx->typing->constraints.constraint_count ||
			 ctx->typing->constraints.constraints[origin_constraint_id].domain !=
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION))) {
		return -1;
	}
	struct prototype_typing_constraint constraint = {
		.domain = OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW,
		.kind = kind,
		.origin_constraint_id = origin_constraint_id,
		.classifier_rhs_root = PROTOTYPE_INVALID_ID,
		.payload.effect = {
			.result_row = result_row,
			.left_row = left_row,
			.right_row = right_row
		}
	};
	uint32_t previous_constraint_count =
		ctx->typing->constraints.constraint_count;
	uint32_t id;
	if (operation_solver_intern_constraint(
			ctx, operation, &constraint, NULL, 0, &id
		) != 0) return -1;
	if (id >= previous_constraint_count) {
		ctx->typing->constraints.next_effect_constraint[id] =
			ctx->typing->constraints.first_effect_constraint_for_occurrence[
				operation
			];
		ctx->typing->constraints.first_effect_constraint_for_occurrence[operation] =
			id;
	}
	if (ctx->terms->terms[result_row].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		uint32_t typed_projection;
		if (operation_solver_canonical_typed_projection(
				ctx, operation, &typed_projection
			) != 0 || typed_projection >=
				PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
			return -1;
		}
		uint32_t answer_id = ctx->typing->constraints.
			effect_solution_constraint_for_projection[typed_projection];
		if (answer_id == PROTOTYPE_INVALID_ID) {
			struct prototype_typing_constraint answer = {
				.domain = OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW,
				.kind = PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION,
				.origin_constraint_id = PROTOTYPE_INVALID_ID,
				.classifier_rhs_root = PROTOTYPE_INVALID_ID,
				.payload.effect = {
					.result_row = result_row,
					.left_row = result_row,
					.right_row = PROTOTYPE_INVALID_ID
				}
			};
			previous_constraint_count = ctx->typing->constraints.constraint_count;
			if (operation_solver_intern_projected_constraint(
					ctx, typed_projection, &answer, NULL, 0, &answer_id
				) != 0) {
				return -1;
			}
			if (answer_id >= previous_constraint_count) {
				ctx->typing->constraints.next_effect_constraint[answer_id] =
					ctx->typing->constraints.
						first_effect_constraint_for_occurrence[operation];
				ctx->typing->constraints.
					first_effect_constraint_for_occurrence[operation] = answer_id;
			}
			ctx->typing->constraints.
				effect_solution_constraint_for_projection[typed_projection] =
				answer_id;
		} else if (answer_id >= ctx->typing->constraints.constraint_count ||
			ctx->typing->constraints.constraints[answer_id].domain !=
				OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW ||
			ctx->typing->constraints.constraints[answer_id].kind !=
				PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION ||
			ctx->typing->constraints.constraints[answer_id].payload.effect.result_row !=
				result_row) {
			return -1;
		}
	}
	return 0;
}

static int operation_classifier_computation_view(
	struct compile_context* ctx,
	uint32_t operation,
	struct prototype_classifier_view* p_view
) {
	if (!ctx || !p_view || operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t classifier = operation_available_classifier(ctx, operation);
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			classifier,
			p_view
		) != 0) {
		return -1;
	}
	return p_view->category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
		p_view->computation_kind ==
			PROTOTYPE_COMPUTATION_KIND_RETURNING &&
		p_view->effect_row != PROTOTYPE_INVALID_ID ? 0 : 1;
}

static uint32_t operation_effect_stable_row(
	const struct compile_context* ctx,
	uint32_t owner_occurrence,
	uint32_t current_row
) {
	if (!ctx || owner_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t typed_projection;
	if (operation_solver_canonical_typed_projection(
			ctx, owner_occurrence, &typed_projection
		) != 0 || typed_projection >=
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t answer_id = ctx->typing->constraints.
		effect_solution_constraint_for_projection[typed_projection];
	if (answer_id != PROTOTYPE_INVALID_ID &&
		answer_id < ctx->typing->constraints.constraint_count) {
		const struct prototype_typing_constraint* answer =
			&ctx->typing->constraints.constraints[answer_id];
		if (answer->domain == OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			answer->kind == PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
			return answer->payload.effect.result_row;
		}
	}
	return current_row;
}

static int operation_effect_solution_equation_for_occurrence(
	const struct compile_context* ctx,
	uint32_t owner_occurrence,
	uint32_t* p_answer_id
) {
	uint32_t typed_projection;
	if (!ctx || !p_answer_id || operation_solver_canonical_typed_projection(
			ctx, owner_occurrence, &typed_projection
		) != 0 || typed_projection >=
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return -1;
	}
	uint32_t answer_id = ctx->typing->constraints.
		effect_solution_constraint_for_projection[typed_projection];
	if (answer_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (answer_id >= ctx->typing->constraints.constraint_count ||
		ctx->typing->constraints.constraints[answer_id].domain !=
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW ||
		ctx->typing->constraints.constraints[answer_id].kind !=
			PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
		return -1;
	}
	*p_answer_id = answer_id;
	return 0;
}

static int operation_effect_normal_add_atom(
	const struct prototype_term_db* terms,
	struct prototype_effect_row_normal_form* normal,
	uint32_t atom
) {
	if (!terms || !normal || atom >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		uint32_t existing = normal->atoms[i];
		if (existing >= terms->term_count) {
			return -1;
		}
		if (existing == atom) {
			return 0;
		}
	}
	if (normal->atom_count >= PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	normal->atoms[normal->atom_count++] = atom;
	return 0;
}

static int operation_effect_normal_union(
	const struct prototype_term_db* terms,
	struct prototype_effect_row_normal_form* target,
	const struct prototype_effect_row_normal_form* source
) {
	if (!target || !source) {
		return -1;
	}
	for (uint32_t i = 0; i < source->atom_count; ++i) {
		if (operation_effect_normal_add_atom(
				terms, target, source->atoms[i]
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_normal_equal(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* left,
	const struct prototype_effect_row_normal_form* right
) {
	return left && right &&
		prototype_term_effect_row_normal_form_includes(terms, left, right) &&
		prototype_term_effect_row_normal_form_includes(terms, right, left);
}


static int operation_classifier_canonicalize_effect_rows(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t depth,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || classifier >= ctx->terms->term_count ||
		depth > 64) {
		return -1;
	}
	const struct prototype_term* term = &ctx->terms->terms[classifier];
	switch (term->tag) {
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
			struct prototype_effect_row_normal_form normal;
			uint32_t row;
			return prototype_term_effect_row_normal_form(
					ctx->terms, term->as.computation_type.label, &normal
				) != 0 || prototype_term_effect_row_materialize_normal_form(
					ctx->terms, &normal, &row
				) != 0 ? -1 : prototype_term_computation_type(
					ctx->terms,
					row,
					term->as.computation_type.result,
					term->as.computation_type.totality,
					p_classifier
				);
		}
		case PROTOTYPE_TERM_THUNK_TYPE: {
			uint32_t computation;
			return operation_classifier_canonicalize_effect_rows(
					ctx,
					term->as.thunk_type.computation,
					depth + 1,
					&computation
				) != 0 ? -1 : prototype_term_thunk_type(
					ctx->terms, computation, p_classifier
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t body;
			return operation_classifier_canonicalize_effect_rows(
					ctx,
					term->as.effect_row_forall.body,
					depth + 1,
					&body
				) != 0 ? -1 : prototype_term_effect_row_forall(
					ctx->terms,
					term->as.effect_row_forall.binding_id,
					body,
					p_classifier
				);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t binder;
			uint32_t body;
			uint32_t family;
			return operation_classifier_canonicalize_effect_rows(
					ctx, term->as.pi.domain, depth + 1, &domain
				) != 0 || prototype_term_pure_family_parts(
					ctx->terms, term->as.pi.codomain_family, &binder, &body
				) != 0 || operation_classifier_canonicalize_effect_rows(
					ctx, body, depth + 1, &body
				) != 0 || prototype_term_pure_family(
					ctx->terms, binder, body, &family
				) != 0 ? -1 : prototype_term_pi_family(
					ctx->terms, domain, family, p_classifier
				);
		}
		default:
			*p_classifier = classifier;
			return 0;
	}
}

/* Effect equations remain immutable inputs. Once an occurrence-owned row
 * equation has a materialized answer, every classifier projection derived
 * from that input is read through the same substitution. This prevents a
 * later source-seed replay from replacing a solved projection with its raw
 * placeholder while keeping the source equation available for checkpoints. */
static int operation_effect_apply_materialized_solution(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t classifier,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->typing || !p_classifier ||
		classifier >= ctx->terms->term_count) {
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
	uint32_t answer_id = typed_projection <
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ?
		ctx->typing->constraints.effect_solution_constraint_for_projection[
			typed_projection
		] : PROTOTYPE_INVALID_ID;
	if (answer_id == PROTOTYPE_INVALID_ID) {
		uint32_t canonical_projection;
		if (operation_solver_canonical_typed_projection(
				ctx, projection->source_occurrence, &canonical_projection
			) != 0 || canonical_projection >=
				PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
			return -1;
		}
		answer_id = ctx->typing->constraints.
			effect_solution_constraint_for_projection[canonical_projection];
	}
	if (answer_id == PROTOTYPE_INVALID_ID) {
		*p_classifier = classifier;
		return 0;
	}
	if (answer_id >= ctx->typing->constraints.constraint_count) {
		return -1;
	}
	const struct prototype_typing_constraint* answer =
		&ctx->typing->constraints.constraints[answer_id];
	const struct prototype_typing_constraint_solution* solution =
		&ctx->typing->constraints.solutions[answer_id];
	if (answer->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW ||
		answer->kind != PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
		return -1;
	}
	if (solution->result_term == PROTOTYPE_INVALID_ID) {
		*p_classifier = classifier;
		return 0;
	}
	uint32_t binding_id = ctx->terms->terms[
		answer->payload.effect.result_row
	].as.effect_row_var.binding_id;
	uint32_t substituted;
	return prototype_term_graph_substitute_bound_var(
			ctx->terms,
			classifier,
			binding_id,
			solution->result_term,
			&substituted
		) != 0 ? -1 : operation_classifier_canonicalize_effect_rows(
			ctx, substituted, 0, p_classifier
		);
}

static int operation_effect_solution_row(
	struct compile_context* ctx,
	uint32_t owner_typed_projection,
	uint32_t placeholder_row,
	uint32_t* p_solution_row
) {
	if (!ctx || !p_solution_row || placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[placeholder_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t found = PROTOTYPE_INVALID_ID;
	uint32_t preferred = owner_typed_projection <
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ?
		ctx->typing->constraints.effect_solution_constraint_for_projection[
			owner_typed_projection
		] : PROTOTYPE_INVALID_ID;
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints, OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint* equation =
			&ctx->typing->constraints.constraints[id];
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (equation->kind != PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION ||
			equation->payload.effect.result_row != placeholder_row ||
			solution->result_term == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (found != PROTOTYPE_INVALID_ID) {
			struct prototype_effect_row_normal_form left;
			struct prototype_effect_row_normal_form right;
			if (prototype_term_effect_row_normal_form(
					ctx->terms, found, &left
				) != 0 || prototype_term_effect_row_normal_form(
					ctx->terms, solution->result_term, &right
				) != 0 || !operation_effect_normal_equal(
					ctx->terms, &left, &right
				)) {
				return -1;
			}
		}
		found = solution->result_term;
		if (id == preferred) {
			/* Continue the scan: a shared placeholder must not acquire two
			 * incompatible projection-local answers. */
			found = solution->result_term;
		}
	}
	if (found == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_solution_row = found;
	return 0;
}

static int operation_effect_resolve_row_normal(
	struct compile_context* ctx,
	uint32_t owner_typed_projection,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
) {
	struct prototype_effect_row_normal_form source;
	if (!ctx || !p_normal ||
		prototype_term_effect_row_normal_form(ctx->terms, row, &source) != 0) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	for (uint32_t i = 0; i < source.atom_count; ++i) {
		uint32_t atom = source.atoms[i];
		uint32_t solved_row = PROTOTYPE_INVALID_ID;
		if (atom < ctx->terms->term_count &&
			ctx->terms->terms[atom].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			int status = operation_effect_solution_row(
				ctx, owner_typed_projection, atom, &solved_row
			);
			if (status < 0) {
				return -1;
			}
		}
		if (solved_row != PROTOTYPE_INVALID_ID) {
			struct prototype_effect_row_normal_form solution;
			if (prototype_term_effect_row_normal_form(
					ctx->terms, solved_row, &solution
				) != 0 || operation_effect_normal_union(
					ctx->terms, p_normal, &solution
				) != 0) {
				return -1;
			}
		} else if (operation_effect_normal_add_atom(
				ctx->terms, p_normal, atom
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_publish_solution(
	struct compile_context* ctx,
	uint32_t answer_id,
	const struct prototype_effect_row_normal_form* normal,
	uint32_t producer_constraint_id,
	int* p_changed
) {
	if (!ctx || !normal || answer_id >=
			ctx->typing->constraints.constraint_count ||
		producer_constraint_id >= ctx->typing->constraints.constraint_count) {
		return -1;
	}
	const struct prototype_typing_constraint* answer =
		&ctx->typing->constraints.constraints[answer_id];
	struct prototype_typing_constraint_solution* solution =
		&ctx->typing->constraints.solutions[answer_id];
	if (answer->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW ||
		answer->kind != PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION ||
		answer->payload.effect.result_row >= ctx->terms->term_count ||
		ctx->terms->terms[answer->payload.effect.result_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t replacement;
	if (prototype_term_effect_row_materialize_normal_form(
			ctx->terms, normal, &replacement
		) != 0) {
		return -1;
	}
	if (solution->result_term != PROTOTYPE_INVALID_ID) {
		struct prototype_effect_row_normal_form existing;
		if (prototype_term_effect_row_normal_form(
				ctx->terms, solution->result_term, &existing
			) != 0 || !operation_effect_normal_equal(
				ctx->terms, &existing, normal
			)) {
			(void)prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				answer_id,
				OPERATION_CONSTRAINT_STATE_CONTRADICTION,
				OPERATION_CLASSIFIER_GOAL_REASON_NONE
			);
			return -1;
		}
		if (getenv("A_PROGRAM_EFFECT_TRACE") != NULL) {
			fprintf(stderr,
				"effect-answer-reused answer=%u producer=%u row=%u revision=%" PRIu64 "\n",
				answer_id, producer_constraint_id, solution->result_term,
				solution->answer_revision
			);
		}
		return 0;
	}
	solution->result_term = replacement;
	solution->answer_state = PROTOTYPE_TYPING_EQUATION_ANSWER_VALIDATED;
	solution->producer_constraint_id = producer_constraint_id;
	solution->answer_revision++;
	if (prototype_typing_constraint_transition(
			&ctx->typing->constraints,
			answer_id,
			OPERATION_CONSTRAINT_STATE_SOLVED,
			OPERATION_CLASSIFIER_GOAL_REASON_NONE
		) != 0) {
		return -1;
	}
	if (p_changed) *p_changed = 1;
	if (getenv("A_PROGRAM_EFFECT_TRACE") != NULL) {
		fprintf(stderr,
			"effect-answer-published answer=%u producer=%u placeholder=%u row=%u revision=%" PRIu64 "\n",
			answer_id, producer_constraint_id,
			answer->payload.effect.result_row, replacement,
			solution->answer_revision
		);
	}
	/* A materialized effect row changes an explicit input consumed by every
	 * PROJECTED_CLASSIFIER equation for this source occurrence. It must wake
	 * those equations; it must never publish their answers itself. */
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		if (!projection || projection->source_occurrence !=
				answer->source_occurrence) {
			continue;
		}
		uint32_t equation_id;
		if (operation_solver_projected_classifier_equation_id(
				ctx, typed_projection, &equation_id
			) != 0 || prototype_typing_constraint_mark_input_changed(
				&ctx->typing->constraints, equation_id
			) != 0 || prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				equation_id,
				OPERATION_CONSTRAINT_STATE_PENDING,
				OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
			) != 0 || operation_solver_enqueue_constraint(
				ctx, equation_id, PROTOTYPE_COMPILE_ENQUEUE_DEPENDENCY
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_row_is_ground(
	const struct prototype_term_db* terms,
	uint32_t row,
	uint32_t depth
) {
	if (!terms || row >= terms->term_count || depth > 256) {
		return 0;
	}
	const struct prototype_term* term = &terms->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return operation_effect_row_is_ground(
				terms, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return operation_effect_row_is_ground(
					terms, term->as.effect_row_union.left, depth + 1
				) && operation_effect_row_is_ground(
					terms, term->as.effect_row_union.right, depth + 1
				);
		default:
			return 0;
	}
}

static int operation_effect_normal_is_ground(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!terms || !normal) {
		return 0;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (!operation_effect_row_is_ground(terms, normal->atoms[i], 0)) {
			return 0;
		}
	}
	return 1;
}

static int operation_effect_solve_constraints(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (uint32_t id = prototype_typing_constraint_first_in_domain(
				&ctx->typing->constraints,
				OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
			);
			id != PROTOTYPE_INVALID_ID;
			id = prototype_typing_constraint_next_in_domain(
				&ctx->typing->constraints, id
			)) {
			struct prototype_typing_constraint* constraint =
				&ctx->typing->constraints.constraints[id];
			struct prototype_typing_constraint_solution* solution =
				&ctx->typing->constraints.solutions[id];
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW) {
				return -1;
			}
			if (constraint->kind ==
					PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
				continue;
			}
			if (solution->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
				continue;
			}
			if ((constraint->kind ==
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT ||
				 constraint->kind ==
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY) &&
				constraint->payload.effect.result_row ==
					constraint->payload.effect.left_row) {
				int unowned = operation_classifier_contains_unowned_effect_row(
					ctx,
					constraint->source_occurrence,
					operation_available_classifier(
						ctx, constraint->source_occurrence
					)
				);
				if (unowned < 0) {
					return -1;
				}
				int has_external_ref = unowned != 0 ?
					operation_subtree_contains_external_ref(
						ctx, constraint->source_occurrence
					) : 0;
				if (has_external_ref < 0) {
					return -1;
				}
				int state = unowned != 0 && has_external_ref != 0 ?
					OPERATION_CONSTRAINT_STATE_RESIDUAL :
					OPERATION_CONSTRAINT_STATE_SOLVED;
				if (prototype_typing_constraint_transition(
						&ctx->typing->constraints,
						id,
						state,
						OPERATION_CLASSIFIER_GOAL_REASON_NONE
					) != 0) {
					return -1;
				}
				continue;
			}
			struct prototype_effect_row_normal_form left;
			struct prototype_effect_row_normal_form right;
			struct prototype_effect_row_normal_form expected;
			struct prototype_effect_row_normal_form actual;
			int deferred = 0;
			if (operation_effect_resolve_row_normal(
					ctx,
					constraint->owner_typed_projection,
					constraint->payload.effect.left_row,
					&left
				) != 0 ||
				(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
				 operation_effect_resolve_row_normal(
					ctx,
					constraint->owner_typed_projection,
					constraint->payload.effect.right_row,
					&right
				 ) != 0) ||
				operation_effect_resolve_row_normal(
					ctx,
					constraint->owner_typed_projection,
					constraint->payload.effect.result_row,
					&actual
				) != 0) {
				return -1;
			}
			memset(&expected, 0, sizeof(expected));
			switch (constraint->kind) {
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT:
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY:
					expected = left;
					break;
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION:
					expected = left;
					if (operation_effect_normal_union(
							ctx->terms, &expected, &right
						) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL:
					for (uint32_t handled_index = 0;
						handled_index < right.atom_count;
						++handled_index) {
						uint32_t handled_atom = right.atoms[handled_index];
						if (handled_atom >= ctx->terms->term_count ||
							ctx->terms->terms[handled_atom].tag !=
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							deferred = 1;
							goto residual_complete;
						}
					}
					for (uint32_t atom_index = 0;
						atom_index < left.atom_count;
						++atom_index) {
						uint32_t atom = left.atoms[atom_index];
						int handled = 0;
						if (atom < ctx->terms->term_count &&
							ctx->terms->terms[atom].tag ==
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							int operation_id = ctx->terms->terms[
								atom
							].as.effect_row_operation.operation_id;
							for (uint32_t handled_index = 0;
								handled_index < right.atom_count;
								++handled_index) {
								uint32_t handled_atom = right.atoms[handled_index];
								handled = handled || ctx->terms->terms[
									handled_atom
								].as.effect_row_operation.operation_id == operation_id;
							}
						}
						if (!handled && operation_effect_normal_add_atom(
								ctx->terms, &expected, atom
							) != 0) {
							return -1;
						}
					}
				residual_complete:
					if (deferred) {
						continue;
					}
					break;
				default:
					return -1;
			}
			if (operation_effect_normal_equal(
					ctx->terms, &actual, &expected
				)) {
				if (prototype_typing_constraint_transition(
						&ctx->typing->constraints,
						id,
						OPERATION_CONSTRAINT_STATE_SOLVED,
						OPERATION_CLASSIFIER_GOAL_REASON_NONE
					) != 0) {
					return -1;
				}
				continue;
			}
			if (constraint->payload.effect.result_row >= ctx->terms->term_count ||
				ctx->terms->terms[constraint->payload.effect.result_row].tag !=
					PROTOTYPE_TERM_EFFECT_ROW_VAR) {
				if (operation_effect_normal_is_ground(ctx->terms, &actual) &&
					operation_effect_normal_is_ground(ctx->terms, &expected)) {
					return -1;
					}
				if (prototype_typing_constraint_transition(
						&ctx->typing->constraints,
						id,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			uint32_t result_binder = ctx->terms->terms[
				constraint->payload.effect.result_row
			].as.effect_row_var.binding_id;
			int owned = operation_effect_row_binder_is_owned(
				ctx, constraint->source_occurrence, result_binder
			);
			if (owned < 0) {
				return -1;
			}
			if (owned) {
				if (prototype_typing_constraint_transition(
						&ctx->typing->constraints,
						id,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			if (!operation_effect_normal_is_ground(ctx->terms, &expected)) {
				if (prototype_typing_constraint_transition(
						&ctx->typing->constraints,
						id,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			if (constraint->owner_typed_projection >=
					PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
				return -1;
			}
			uint32_t answer_id = ctx->typing->constraints.
				effect_solution_constraint_for_projection[
					constraint->owner_typed_projection
				];
			int answer_changed = 0;
			if (answer_id == PROTOTYPE_INVALID_ID ||
				operation_effect_publish_solution(
					ctx, answer_id, &expected, constraint->id, &answer_changed
				) != 0) {
				return -1;
			}
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					id,
					OPERATION_CONSTRAINT_STATE_SOLVED,
					OPERATION_CLASSIFIER_GOAL_REASON_NONE
				) != 0) {
				return -1;
			}
			if (answer_changed) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	for (uint32_t id = finalize ? prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		) : PROTOTYPE_INVALID_ID;
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (solution->state == OPERATION_CONSTRAINT_STATE_PENDING &&
			prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				id,
				OPERATION_CONSTRAINT_STATE_RESIDUAL,
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION
			) != 0) {
			return -1;
		}
	}
	if (operation_constraint_db_validate(ctx) != 0) {
		return -1;
	}
	memset(
		&ctx->metadata->effect_constraint_summary,
		0,
		sizeof(ctx->metadata->effect_constraint_summary)
	);
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (ctx->typing->constraints.constraints[id].kind ==
				PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
			continue;
		}
		if (solution->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->effect_constraint_summary.solved_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->effect_constraint_summary.residual_count++;
		} else if (solution->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			ctx->metadata->effect_constraint_summary.failed_count++;
		}
	}
	return 0;
}

static int operation_effect_import_judgement_constraint(
	struct compile_context* ctx,
	const struct prototype_judgement_effect_row_constraint* source
) {
	if (!ctx || !source || source->operand_count != 2 ||
		source->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION ||
		source->subject >= ctx->terms->term_count ||
		source->result_row >= ctx->terms->term_count ||
		source->operands[0] >= ctx->terms->term_count ||
		source->operands[1] >= ctx->terms->term_count) {
		return source && source->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION ? 0 : -1;
	}
	if (source->computation_constraint_id == PROTOTYPE_INVALID_ID) {
		/* Authority-neutral Core helper equations do not own a source occurrence
		 * and therefore cannot become elaboration constraints. */
		return 0;
	}
	if (source->computation_constraint_id >=
		ctx->judgement_delta.computation_constraint_count) {
		return -1;
	}
	const struct prototype_judgement_computation_constraint* computation =
		&ctx->judgement_delta.computation_constraints[
			source->computation_constraint_id
		];
	if (source->computation_constraint_id >=
			ctx->computation_constraint_input_capacity ||
		ctx->computation_constraint_inputs[
			source->computation_constraint_id
		].subject != source->subject ||
		computation->occurrence_id == PROTOTYPE_INVALID_ID ||
		computation->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t lifecycle_id = PROTOTYPE_INVALID_ID;
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_COMPUTATION
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint* candidate =
			&ctx->typing->constraints.constraints[id];
		if (candidate->domain == OPERATION_CONSTRAINT_DOMAIN_COMPUTATION &&
			candidate->payload.computation.judgement_delta_constraint_id ==
				source->computation_constraint_id) {
			lifecycle_id = id;
			break;
		}
	}
	if (lifecycle_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_classifier_view current_view;
	if (operation_classifier_computation_view(
			ctx, computation->occurrence_id, &current_view
		) != 0) {
		/* The kernel may emit the equation before the occurrence classifier
		 * reaches a computation view. Keep the computation dependency as the
		 * authority and import the row equation on the next fixed-point pass. */
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			current_view.effect_row,
			source->result_row
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The equation belongs to an older fixed-point operand snapshot. Its
		 * owner is known, but it is not a premise of the current projection. */
		return 0;
	}
	int kind = source->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN ?
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION :
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL;
	return operation_effect_constraint_add(
		ctx,
		kind,
		computation->occurrence_id,
		source->result_row,
		source->operands[0],
		source->operands[1],
		lifecycle_id
	);
}

static int operation_effect_generate_constraints_impl(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* Build immutable equations over stable EffectRow placeholders. Repeated
	 * calls only rediscover the same canonical edges while solutions advance. */
	uint32_t empty_row;
	if (prototype_term_effect_row_empty(ctx->terms, &empty_row
		) != 0) {
		return -1;
	}
	uint32_t transaction_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t operation_id = transaction_start;
		ctx->classifier_workspace.initialized &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		struct prototype_classifier_view result_view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &result_view
			) != 0) {
			continue;
		}
		uint32_t result_row = operation_effect_stable_row(
			ctx, operation_id, result_view.effect_row
		);
		if (result_row == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN) {
			if (operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT,
					operation_id,
					result_row,
					empty_row,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE) {
			uint32_t child = operation->wrapped_occurrence;
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE &&
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0,
					&child
				) != 0) {
				return -1;
			}
			struct prototype_classifier_view child_view;
			if (operation_classifier_computation_view(
					ctx, child, &child_view
				) == 0 && operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
					operation_id,
						result_row,
						operation_effect_stable_row(
							ctx, child, child_view.effect_row
						),
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			operation->case_count != 0) {
			uint32_t accumulated = PROTOTYPE_INVALID_ID;
			for (uint32_t case_index = 0;
				case_index < operation->case_count;
				++case_index) {
				uint32_t branch_operation;
				if (compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
						case_index, &branch_operation
					) != 0) {
					return -1;
				}
				struct prototype_classifier_view case_view;
				if (operation_classifier_computation_view(
						ctx, branch_operation, &case_view
					) != 0) {
					accumulated = PROTOTYPE_INVALID_ID;
					break;
				}
				if (accumulated == PROTOTYPE_INVALID_ID) {
						accumulated = operation_effect_stable_row(
							ctx, branch_operation, case_view.effect_row
						);
					continue;
				}
				uint32_t branch_row = operation_effect_stable_row(
					ctx, branch_operation, case_view.effect_row
				);
				uint32_t union_row;
				if (prototype_term_effect_row_union(
						ctx->terms,
						accumulated,
						branch_row,
						&union_row
					) != 0 || operation_effect_constraint_add(
						ctx,
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION,
						operation_id,
						union_row,
						accumulated,
						branch_row,
						PROTOTYPE_INVALID_ID
					) != 0) {
					return -1;
				}
				accumulated = union_row;
			}
			if (accumulated != PROTOTYPE_INVALID_ID &&
				operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_row,
					accumulated,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		}
		if (ctx->typing->constraints.first_effect_constraint_for_occurrence[
				operation_id
			] == PROTOTYPE_INVALID_ID && operation_effect_constraint_add(
				ctx,
				PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
				operation_id,
				result_row,
				result_row,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
	}
	for (size_t constraint_id = 0;
		constraint_id < ctx->judgement_delta.effect_row_constraint_count;
		++constraint_id) {
		const struct prototype_judgement_effect_row_constraint* row_constraint =
			&ctx->judgement_delta.effect_row_constraints[constraint_id];
		if (row_constraint->operand_count != 2 || row_constraint->kind ==
				PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			continue;
		}
		if (operation_effect_import_judgement_constraint(
				ctx, row_constraint
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_generate_constraints(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx) {
		return -1;
	}
	if (ctx->typing->constraints.topology_sealed) {
		ctx->metadata->proof_solver_call_by_domain[
			PROTOTYPE_COMPILE_SOLUTION_EFFECT
		]++;
		return -1;
	}
	struct prototype_typing_constraint_db_mark mark =
		prototype_typing_constraint_db_mark(&ctx->typing->constraints);
	int status = operation_effect_generate_constraints_impl(ctx);
	if (status != 0 && prototype_typing_constraint_db_rollback(
			&ctx->typing->constraints, mark
		) != 0) {
		return -1;
	}
	return status == 0 ? operation_effect_solve_constraints(ctx, finalize) : status;
}

static size_t operation_effect_unresolved_count(
	const struct prototype_typing_constraint_db* constraints
) {
	if (!constraints) {
		return 0;
	}
	size_t count = 0;
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			constraints, OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(constraints, id)) {
		if (constraints->constraints[id].kind ==
				PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
			continue;
		}
		if (constraints->solutions[id].state !=
			OPERATION_CONSTRAINT_STATE_SOLVED) {
			count++;
		}
	}
	return count;
}

static int compile_phase_record_residual_effect_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || !ctx->terms) {
		return -1;
	}
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		if (constraint->kind ==
				PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION) {
			continue;
		}
		if (ctx->typing->constraints.solutions[id].state ==
			OPERATION_CONSTRAINT_STATE_SOLVED) {
			continue;
		}
		if (constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			constraint->payload.effect.result_row >= ctx->terms->term_count ||
			constraint->payload.effect.left_row >= ctx->terms->term_count ||
			(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
			 constraint->payload.effect.right_row >= ctx->terms->term_count)) {
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[
				constraint->source_occurrence
			];
		int already_recorded = 0;
		uint32_t obligation_id = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0;
			j < prototype_verification_db_count(&ctx->metadata->verification);
			++j) {
			const struct prototype_verification_obligation* existing =
				prototype_verification_db_get(&ctx->metadata->verification, j);
			if (existing && existing->kind ==
					PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION &&
				existing->occurrence == constraint->source_occurrence &&
				existing->effect_constraint_kind == constraint->kind &&
				existing->effect_row == constraint->payload.effect.result_row &&
				existing->input_classifier == constraint->payload.effect.left_row &&
				existing->classifier_family ==
					constraint->payload.effect.right_row) {
				already_recorded = 1;
				obligation_id = j;
				break;
			}
		}
		if (!already_recorded && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation) {
					.kind =
						PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = constraint->source_occurrence,
					.core_term = occurrence->core_term,
					.computation_occurrence = PROTOTYPE_INVALID_ID,
					.continuation_occurrence = PROTOTYPE_INVALID_ID,
					.continuation_binder_id = PROTOTYPE_INVALID_ID,
					.input_classifier = constraint->payload.effect.left_row,
					.classifier_family = constraint->payload.effect.right_row,
					.effect_row = constraint->payload.effect.result_row,
					.effect_constraint_kind = constraint->kind,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF
				},
				&obligation_id
			) != 0) {
			return -1;
		}
		if (obligation_id == PROTOTYPE_INVALID_ID ||
			prototype_verification_db_add_dependency(
				&ctx->metadata->verification,
				constraint->source_occurrence,
				obligation_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_require_complete(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t constraint_id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
		constraint_id != PROTOTYPE_INVALID_ID;
		constraint_id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, constraint_id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[constraint_id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			ctx->typing->constraints.solutions[constraint_id].state !=
				OPERATION_CONSTRAINT_STATE_RESIDUAL ||
			ctx->typing->constraints.solutions[constraint_id].branch_refinement.
				refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			continue;
		}
		const struct prototype_typed_occurrence* branch =
			constraint->source_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count ?
				&ctx->metadata->typed_occurrences.occurrences[
					constraint->source_occurrence
				] : NULL;
		uint32_t source_ast = branch ? branch->source_ast :
			constraint->source_ast;
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			PROTOTYPE_COMPILE_DIAGNOSTIC_BRANCH_REFINEMENT_RESIDUAL,
			source_ast,
			constraint->source_occurrence,
			constraint_id,
			ctx->typing->constraints.solutions[constraint_id].branch_refinement.
				residual_pattern,
			ctx->typing->constraints.solutions[constraint_id].branch_refinement.
				residual_value,
			compile_source_span(ctx, source_ast)
		);
		return -1;
	}
	/* A failed motive equation is the cause of any unclassified enclosing
	 * Lambda/APP chain. Diagnose that authoritative equation before reporting a
	 * derived parent as generically unsolved. */
	for (uint32_t constraint_id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		constraint_id != PROTOTYPE_INVALID_ID;
		constraint_id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, constraint_id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[constraint_id];
		const struct prototype_typing_constraint_solution* equation_solution =
			&ctx->typing->constraints.solutions[constraint_id];
		if (constraint->kind == OPERATION_CONSTRAINT_PI_EXPECTED &&
			constraint->payload.classifier.goal.pi.role ==
				OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
			equation_solution->state ==
				OPERATION_CONSTRAINT_STATE_CONTRADICTION &&
			equation_solution->reason ==
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_DOMAIN_CONTRADICTION &&
			equation_solution->contradiction_expected_classifier <
				ctx->terms->term_count &&
			equation_solution->contradiction_actual_classifier <
				ctx->terms->term_count) {
			const struct prototype_typed_occurrence* operation =
				constraint->source_occurrence <
					ctx->metadata->typed_occurrences.occurrence_count ?
					&ctx->metadata->typed_occurrences.occurrences[
						constraint->source_occurrence
					] : NULL;
			uint32_t source_ast = operation ? operation->source_ast :
				constraint->source_ast;
			(void)add_compile_diagnostic(
				ctx,
				PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
				PROTOTYPE_COMPILE_DIAGNOSTIC_CLASSIFIER_EQUATION_CONTRADICTION,
				source_ast,
				constraint->source_occurrence,
				constraint_id,
				equation_solution->contradiction_expected_classifier,
				equation_solution->contradiction_actual_classifier,
				compile_source_span(ctx, source_ast)
			);
			return -1;
		}
		if (constraint->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION) {
			continue;
		}
		uint32_t match_projection = constraint->owner_typed_projection;
		if (match_projection >=
				ctx->typing->context_projections.typed_projection_count) {
			return -1;
		}
		uint32_t motive = ctx->typing->constraints.
			motive_solution_for_projection[match_projection].motive;
		if (motive >= ctx->terms->term_count ||
			equation_solution->validated_motive != motive ||
			equation_solution->validation_status !=
				OPERATION_MOTIVE_EQUATION_MISMATCH) {
			continue;
		}
		/* Completion reports the answer already owned by this equation. Running
		 * conversion here would create a second, terminal-only Solver path and
		 * make checkpoint resumption observably different from an in-memory run. */
		(void)operation_motive_add_terminal_diagnostic(
			ctx,
			constraint->source_occurrence,
			match_projection,
			constraint->payload.classifier.goal.motive_case.case_index
		);
		return -1;
	}
	uint32_t transaction_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projected =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		if (!projected || projected->source_occurrence < transaction_start ||
			projected->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			continue;
		}
		const struct prototype_context_projection_solution* projected_context =
			prototype_context_projection_solution_get(
				&ctx->typing->context_projections,
				projected->context_projection
			);
		/* Reachability belongs to an exact TypedProjection, not to its shared
		 * Layer C occurrence. The same erased Lambda or APP may occur in both a
		 * reachable branch and a branch whose refined Context is contradictory. */
		if (projected_context && projected_context->state ==
				PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
			continue;
		}
		uint32_t equation_id =
			ctx->typing->constraints.classifier_constraint_for_projection[
				typed_projection
			];
		if (!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, equation_id,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			)) {
			continue;
		}
		uint32_t i = projected->source_occurrence;
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		int impossible_branch = operation_solver_occurrence_is_in_impossible_branch(
			ctx, i
		);
		if (impossible_branch < 0) {
			return -1;
		}
		if (impossible_branch > 0) {
			continue;
		}
		switch (operation->tag) {
			case PROTOTYPE_TYPED_OCCURRENCE_APP:
			case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
				if (operation_solver_classifier_for_projection(
						ctx, typed_projection
					) ==
					PROTOTYPE_INVALID_ID) {
					fprintf(stderr,
						"classifier solver incomplete projection=%u equation=%u "
						"occurrence=%u tag=%d ast=%u context=%u core=%u action=%u "
						"ih-owner=%u ih-case=%u ih-field=%u scope=%u\n",
						typed_projection, equation_id, i, operation->tag,
						operation->source_ast,
						operation->context_id, operation->core_term,
						operation->context_action_substitution,
						operation->ih_owner_occurrence,
						operation->ih_case_index, operation->ih_field_index,
						operation->ih_scope_id);
					if (operation->tag ==
							PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
						operation->ih_owner_occurrence <
							ctx->metadata->typed_occurrences.occurrence_count) {
						uint32_t owner = operation->ih_owner_occurrence;
						uint32_t ih_constraint = ctx->typing->constraints.
							ih_constraint_for_projection[typed_projection];
						uint32_t owner_projection = PROTOTYPE_INVALID_ID;
						if (prototype_typing_constraint_has_domain(
								&ctx->typing->constraints, ih_constraint,
								OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
							) &&
							ctx->typing->constraints.constraints[ih_constraint].
								operand_count >= 2) {
							owner_projection =
								operation_solver_constraint_operand_projection(
									ctx,
									&ctx->typing->constraints.constraints[ih_constraint],
									1
								);
						}
						const struct prototype_typed_occurrence* owner_occurrence =
							&ctx->metadata->typed_occurrences.occurrences[owner];
						fprintf(stderr,
							"  IH owner tag=%d ast=%u context=%u core=%u classifier=%u motive=%u motive-state=%u\n",
							owner_occurrence->tag, owner_occurrence->source_ast,
							owner_occurrence->context_id, owner_occurrence->core_term,
							operation_solver_classifier(ctx, owner),
							owner_projection < ctx->typing->context_projections.
								typed_projection_count ? ctx->typing->constraints.
								motive_solution_for_projection[owner_projection].motive :
								PROTOTYPE_INVALID_ID,
							owner_projection < ctx->typing->context_projections.
								typed_projection_count ? ctx->typing->constraints.
								motive_solution_for_projection[owner_projection].phase : 0);
					}
					for (uint32_t edge = 0; edge < operation->edge_count; ++edge) {
						const struct prototype_typed_occurrence_edge* child_edge =
							&ctx->metadata->typed_occurrences.edges[
								operation->first_edge + edge
							];
						uint32_t child = child_edge->child_occurrence;
						fprintf(stderr,
							"  child role=%d ordinal=%u occurrence=%u tag=%d classifier=%u\n",
							child_edge->role, child_edge->ordinal, child,
							child < ctx->metadata->typed_occurrences.occurrence_count ?
								ctx->metadata->typed_occurrences.occurrences[child].tag : -1,
							child < ctx->metadata->typed_occurrences.occurrence_count ?
								operation_solver_classifier(ctx, child) :
								PROTOTYPE_INVALID_ID);
						if (child < ctx->metadata->typed_occurrences.occurrence_count &&
							operation_solver_classifier(ctx, child) !=
								PROTOTYPE_INVALID_ID) {
							fprintf(stderr, "    classifier-term: ");
							prototype_type_term_print_debug(
								stderr, ctx->symbols, ctx->intrinsic_environment,
								ctx->type_declarations, ctx->terms,
								operation_solver_classifier(ctx, child)
							);
							fprintf(stderr, "\n");
						}
					}
					uint32_t debug_first = i > 8 ? i - 8 : 0;
					for (uint32_t debug = debug_first; debug <= i; ++debug) {
						const struct prototype_typed_occurrence* nearby =
							&ctx->metadata->typed_occurrences.occurrences[debug];
						fprintf(stderr,
							"nearby occurrence=%u tag=%d ast=%u core=%u classifier=%u term=",
							debug, nearby->tag, nearby->source_ast, nearby->core_term,
							operation_solver_classifier(ctx, debug));
						prototype_type_term_print_debug(
							stderr, ctx->symbols, ctx->intrinsic_environment,
							ctx->type_declarations, ctx->terms, nearby->core_term
						);
						fprintf(stderr, "\n");
					}
					if (ctx->metadata->compile_diagnostic_count == 0) {
						int reason = PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER;
						uint32_t constraint_id = PROTOTYPE_INVALID_ID;
						uint32_t expected = PROTOTYPE_INVALID_ID;
						uint32_t actual = PROTOTYPE_INVALID_ID;
						for (uint32_t j = prototype_typing_constraint_first_in_domain(
								&ctx->typing->constraints,
								OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
							);
							j != PROTOTYPE_INVALID_ID;
							j = prototype_typing_constraint_next_in_domain(
								&ctx->typing->constraints, j
							)) {
							const struct prototype_typing_constraint* constraint =
								&ctx->typing->constraints.constraints[j];
							int motive_branch = constraint->kind ==
								OPERATION_CONSTRAINT_MOTIVE_EQUATION &&
								constraint->payload.classifier.goal.motive_case.
									branch_operation == i;
							if (constraint->source_occurrence != i && !motive_branch) {
								continue;
							}
							constraint_id = constraint->id;
							switch (constraint->kind) {
								case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_CONSTRUCTOR_DOMAIN_MISMATCH;
									break;
								case OPERATION_CONSTRAINT_IH_EXPECTED:
									struct prototype_typed_occurrence_induction_edge ih_edge;
									int ih_edge_status =
										prototype_typed_occurrence_graph_induction_edge(
											prototype_compile_metadata_typed_occurrences_const(
												ctx->metadata
											), ctx->terms, i, &ih_edge
										);
									int ih_owner_matches = ih_edge_status == 0 &&
										ih_edge.owner_match_occurrence == constraint->payload.classifier.
											goal.induction_hypothesis.owner_match_occurrence &&
										ih_edge.argument_occurrence == constraint->payload.classifier.
											goal.induction_hypothesis.recursive_argument_occurrence &&
										ih_edge.scope_id == constraint->payload.classifier.goal.
											induction_hypothesis.ih_scope_id &&
										ih_edge.case_index == constraint->payload.classifier.goal.
											induction_hypothesis.case_index &&
										ih_edge.field_index == constraint->payload.classifier.goal.
											induction_hypothesis.field_index;
									reason = ih_owner_matches ?
										PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER :
										PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP;
									break;
									case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
										{
											int diagnostic_status =
											operation_motive_add_terminal_diagnostic(
											ctx,
											constraint->source_occurrence,
											constraint->owner_typed_projection,
											constraint->payload.classifier.goal.motive_case.
													case_index
												);
											if (diagnostic_status < 0) {
												return -1;
											}
											if (diagnostic_status == 0) {
												reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_EQUATION_MISMATCH;
											} else {
												/* This case is valid. Keep scanning the same Match for the
												 * unresolved equation instead of hiding it behind a generic
												 * Match-result diagnostic. */
												reason = PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER;
												continue;
											}
										}
										break;
								case OPERATION_CONSTRAINT_CONVERTIBLE:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH;
									expected = constraint->payload.classifier.goal.conversion.expected_classifier;
									actual = operation_solver_classifier(
										ctx, constraint->payload.classifier.goal.conversion.body_occurrence
									);
									break;
								default:
									break;
							}
							if (reason != PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER) {
								break;
							}
						}
						if (ctx->metadata->compile_diagnostic_count == 0) {
							(void)add_compile_diagnostic(
							ctx,
							PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
							reason,
							operation->source_ast,
							i,
							constraint_id,
							expected,
							actual,
							compile_source_span(ctx, operation->source_ast)
							);
						}
					}
					return -1;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

static int compile_phase_record_residual_dependent_folds(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = ctx->accepted_typing.occurrence_count;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t continuation_occurrence;
		uint32_t fold_projection;
		uint32_t computation_projection;
		uint32_t continuation_projection;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count != 0 ||
			compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&continuation_occurrence
			) != 0) {
			continue;
		}
		fold_projection = operation_solver_publication_projection(ctx, operation_id);
		if (fold_projection == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		/* A closed fold classifier is the authoritative solver answer.  Do not
		 * reconstruct a residual from the fact that normalization of its effectful
		 * input is blocked: structural return projection may already have closed a
		 * dependent continuation without performing that effect. */
		uint32_t fold_classifier = operation_solver_classifier_for_projection(
			ctx, fold_projection
		);
		if (fold_classifier != PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (
			operation_solver_projected_child(
				ctx, fold_projection, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_projection
			) != 0 || operation_solver_projected_child(
				ctx, fold_projection, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&continuation_projection
			) != 0) {
			return -1;
		}
		uint32_t input_classifier = operation_solver_classifier_for_projection(
			ctx, computation_projection
		);
		uint32_t continuation_classifier = operation_solver_classifier_for_projection(
			ctx, continuation_projection
		);
		struct prototype_classifier_view input_view;
		uint32_t domain;
		uint32_t classifier_family;
		uint32_t continuation_binder_id;
		uint32_t codomain;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			continuation_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_COMPUTATION_KIND_RETURNING ||
			prototype_judgement_pi_parts(
				ctx->terms, continuation_classifier, &domain, &classifier_family
			) != 0 || !(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, domain, input_view.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_pure_family_parts(
				ctx->terms, classifier_family, &continuation_binder_id, &codomain
			) != 0 || !prototype_term_contains_free_binding(
				ctx->terms, codomain, continuation_binder_id
			)) {
			continue;
		}
		struct prototype_term fold_term;
		if (typing_core_inspect_term(
				ctx->core_requests, operation->core_term, &fold_term
			) != 0 || fold_term.tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
			return -1;
		}
		struct prototype_core_normalization_request request = {
			.term_id = fold_term.as.computation_fold.computation,
			.profile = PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			.step_limit = compile_effort_remaining(ctx->metadata)
		};
		struct prototype_core_normalization_response normalized;
		int effort_status = compile_effort_charge(
			ctx->metadata, ctx->effort_atomic, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
		);
		if (effort_status != 0) return effort_status;
		if (prototype_core_request_normalize(
				ctx->core_requests, &request, &normalized
			) != 0 || normalized.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			continue;
		}
		if (normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT &&
			normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_occurrence(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		int already_recorded = find_status == 0;
		if (!already_recorded && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = operation_id,
					.core_term = operation->core_term,
					.computation_occurrence = computation_occurrence,
					.continuation_occurrence = continuation_occurrence,
					.continuation_binder_id = continuation_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF
				},
				&existing_obligation
			) != 0) {
			return -1;
		}
		if (prototype_verification_db_add_dependency(
				&ctx->metadata->verification,
				operation_id,
				existing_obligation
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int compile_phase_record_residual_computation_fold_results(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = ctx->accepted_typing.occurrence_count;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t return_operation;
		uint32_t return_body_occurrence;
		uint32_t fold_projection;
		uint32_t computation_projection;
		uint32_t return_projection;
		uint32_t return_body_projection;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&return_operation
			) != 0 || compile_occurrence_child(
				ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
				&return_body_occurrence
			) != 0 ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
			continue;
		}
		fold_projection = operation_solver_publication_projection(ctx, operation_id);
		if (fold_projection == PROTOTYPE_INVALID_ID ||
			operation_solver_projected_child(
				ctx, fold_projection, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_projection
			) != 0 || operation_solver_projected_child(
				ctx, fold_projection, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&return_projection
			) != 0 || operation_solver_projected_child(
				ctx, return_projection, PROTOTYPE_TERM_CHILD_BODY, 0,
				&return_body_projection
			) != 0) {
			return -1;
		}
		uint32_t input_classifier = operation_solver_classifier_for_projection(
			ctx, computation_projection
		);
		uint32_t return_classifier = operation_solver_classifier_for_projection(
			ctx, return_body_projection
		);
		struct prototype_classifier_view input_view;
		int return_depends_on_result = return_classifier != PROTOTYPE_INVALID_ID &&
			prototype_term_contains_free_binding(
				ctx->terms, return_classifier, operation->fold_return_binder_id
			);
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			return_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_COMPUTATION_KIND_RETURNING ||
			!return_depends_on_result) {
			continue;
		}
		uint32_t classifier_family;
		if (prototype_term_pure_family(
				ctx->terms,
				operation->fold_return_binder_id,
				return_classifier,
				&classifier_family
			) != 0) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_occurrence(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		if (find_status > 0 && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = operation_id,
					.core_term = operation->core_term,
					.computation_occurrence = computation_occurrence,
					.continuation_occurrence = return_body_occurrence,
					.continuation_binder_id =
						operation->fold_return_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF
				},
				&existing_obligation
			) != 0) {
			return -1;
		}
		if (prototype_verification_db_add_dependency(
				&ctx->metadata->verification,
				operation_id,
				existing_obligation
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_generalize_lambda_effect_rows(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!ctx || !ctx->metadata || !p_ret || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		operation->implicit_effect_row_count > 16) {
		return -1;
	}
	for (uint32_t i = operation->implicit_effect_row_count; i > 0; --i) {
		if (prototype_term_effect_row_forall(
					ctx->terms,
					operation->implicit_effect_row_binders[i - 1],
					classifier,
					&classifier
				) != 0) {
			return -1;
		}
	}
	*p_ret = classifier;
	return 0;
}

static int operation_classifier_lambda_intro_result(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t domain,
	uint32_t body_classifier,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		domain >= ctx->terms->term_count ||
		body_classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		operation->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t codomain_family;
	uint32_t classifier;
	if (prototype_term_pure_family(
			ctx->terms,
			operation->binding_id,
			body_classifier,
			&codomain_family
		) != 0 || prototype_term_pi_family(
			ctx->terms, domain, codomain_family, &classifier
		) != 0 || operation_solver_generalize_lambda_effect_rows(
			ctx, operation_id, classifier, &classifier
		) != 0) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}
