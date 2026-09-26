static int operation_solver_resolve_contexts(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->typing || !ctx->metadata ||
		ctx->metadata->contexts.context_count > PROTOTYPE_CONTEXT_CAPACITY) {
		return -1;
	}
	ctx->metadata->context_resolution_request_count++;
	ctx->metadata->context_resolution_pass_count++;
	if (operation_binder_owners_ensure(ctx) != 0) {
		return -1;
	}
	int reindex_changed;
	if (operation_solver_solve_context_classifier_reindexes(
			ctx, &reindex_changed
		) != 0) {
		return -1;
	}
	(void)reindex_changed;

	size_t context_count = ctx->metadata->contexts.context_count;
	int trace = getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE") != NULL;
	ctx->metadata->full_scan_count[PROTOTYPE_COMPILE_SCAN_CONTEXT]++;
	ctx->metadata->full_scan_visit_count[PROTOTYPE_COMPILE_SCAN_CONTEXT] +=
		context_count;
	for (uint32_t context_id = 1; context_id < context_count; ++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context || context->parent >= context_id) {
			return -1;
		}
		uint32_t classifier = operation_context_classifier_term(ctx, context_id);
		if (trace) {
			const struct prototype_context_classifier_equation* equation =
				prototype_context_classifier_equation_get(
					&ctx->metadata->contexts,
					prototype_context_classifier_equation(context)
				);
			fprintf(
				stderr,
				"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 context=%u parent=%u "
				"binding=%u classifier=%u equation=%u owner=%u source-ast=%u\n",
				context_id, context->parent, context->binding_id, classifier,
				prototype_context_classifier_equation(context),
				equation ? equation->owner_occurrence : PROTOTYPE_INVALID_ID,
				equation ? equation->source_ast_binder_id : PROTOTYPE_INVALID_ID
			);
		}
	}

	/* Source Contexts are schematic construction inputs. One source Lambda may
	 * be reused by several typed projections whose binder classifiers differ,
	 * so its source equation need not have one concrete answer. Materialize every
	 * currently solvable typed projection during the fixed point; `finalize` only
	 * rejects projections which still wait after that fixed point has closed. */
	for (uint32_t typed_projection_id = 0;
		typed_projection_id <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection_id) {
		const struct prototype_typed_projection* typed_projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection_id
			);
		uint32_t concrete_context;
		uint32_t substitution;
		if (!typed_projection || !typed_projection->topology_expanded) {
			continue;
		}
		int status = operation_typed_projection_materialized_context(
			ctx, typed_projection_id, &concrete_context, &substitution
		);
		if (status == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
			continue;
		}
		if (status != 0) {
			if (finalize && trace) {
				const struct prototype_typed_occurrence* occurrence =
					&ctx->metadata->typed_occurrences.occurrences[
						typed_projection->source_occurrence
					];
				fprintf(
					stderr,
					"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 unresolved-projection=%u "
					"occurrence=%u status=%d tag=%d context=%u core=%u source-ast=%u\n",
					typed_projection_id, typed_projection->source_occurrence, status,
					occurrence->tag, occurrence->context_id, occurrence->core_term,
					occurrence->source_ast
				);
			}
			if (finalize) {
				return -1;
			}
			continue;
		}
		(void)concrete_context;
		(void)substitution;
	}
	return 0;
}

static int operation_solver_attach_computation_lifecycle(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t delta_constraint_id,
	int kind
) {
	if (!ctx || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		delta_constraint_id >= ctx->judgement_delta.computation_constraint_count) {
		return -1;
	}
	uint32_t lifecycle_id =
		ctx->typing->constraints.computation_constraint_for_occurrence[
			occurrence_id
		];
	if (lifecycle_id != PROTOTYPE_INVALID_ID) {
		if (lifecycle_id >= ctx->typing->constraints.constraint_count) {
			return -1;
		}
		const struct prototype_typing_constraint* lifecycle =
			&ctx->typing->constraints.constraints[lifecycle_id];
		if (lifecycle->domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			lifecycle->source_occurrence != occurrence_id ||
			lifecycle->payload.computation.judgement_delta_constraint_id !=
				delta_constraint_id) {
			return -1;
		}
		return 0;
	}
	struct prototype_typing_constraint lifecycle = {
		.domain = OPERATION_CONSTRAINT_DOMAIN_COMPUTATION,
		.kind = kind,
		.origin_constraint_id = PROTOTYPE_INVALID_ID,
		.classifier_rhs_root = PROTOTYPE_INVALID_ID,
		.payload.computation = {
			.judgement_delta_constraint_id = delta_constraint_id
		}
	};
	if (operation_solver_intern_constraint(
			ctx, occurrence_id, &lifecycle, NULL, 0, &lifecycle_id
		) != 0) {
		return -1;
	}
	ctx->typing->constraints.solutions[lifecycle_id].reason =
		OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY;
	ctx->typing->constraints.computation_constraint_for_occurrence[
		occurrence_id
	] = lifecycle_id;
	uint32_t owner_typed_projection;
	if (operation_solver_canonical_typed_projection(
			ctx, occurrence_id, &owner_typed_projection
		) != 0) {
		return -1;
	}
	return operation_solver_add_projected_constraint_dependency(
		ctx, owner_typed_projection, lifecycle_id
	);
}

static int operation_solver_generate_computation_constraints_impl(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	uint32_t transaction_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = transaction_start;
		i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			continue;
		}
		uint32_t delta_constraint_id;
		int existing_status =
			prototype_judgement_delta_computation_constraint_for_occurrence(
				&ctx->judgement_delta, i, &delta_constraint_id
			);
		if (existing_status < 0) {
			return -1;
		}
		if (existing_status == 0) {
			const struct prototype_judgement_computation_constraint* existing =
				&ctx->judgement_delta.computation_constraints[delta_constraint_id];
			if (existing->context_id != operation->context_id) {
				fprintf(stderr,
					"computation topology mismatch occurrence=%u delta=%u "
					"context=%u/%u\n",
					i, delta_constraint_id, existing->context_id,
					operation->context_id
				);
				return -1;
			}
			if (operation_solver_attach_computation_lifecycle(
					ctx, i, delta_constraint_id, existing->kind
				) != 0) {
				return -1;
			}
			continue;
		}
		prototype_judgement_delta_set_occurrence(&ctx->judgement_delta, i);
		int status = prototype_judgement_delta_record_computation_constraint(
			&ctx->judgement_delta,
			ctx->terms,
			operation->context_id,
			operation->core_term
		);
		if (status != 0) {
			return -1;
		}
		if (prototype_judgement_delta_computation_constraint_for_occurrence(
				&ctx->judgement_delta, i, &delta_constraint_id
			) != 0) {
			return -1;
		}
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[delta_constraint_id];
		if (operation_solver_attach_computation_lifecycle(
				ctx, i, delta_constraint_id, constraint->kind
			) != 0) {
			return -1;
		}
		uint32_t lifecycle_id =
			ctx->typing->constraints.computation_constraint_for_occurrence[i];
		if (lifecycle_id >= ctx->typing->constraints.constraint_count) {
			return -1;
		}
		uint32_t owner_typed_projection;
		if (operation_solver_canonical_typed_projection(
				ctx, i, &owner_typed_projection
			) != 0) {
			return -1;
		}
		constraint->premise_occurrence_count = 0;
		constraint->return_body_occurrence_id = PROTOTYPE_INVALID_ID;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			uint32_t request_operation;
			uint32_t request_argument;
			uint32_t request_continuation;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&request_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&request_argument
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&request_continuation
				) != 0) {
				return -1;
			}
			constraint->premise_occurrences[0] = request_operation;
			constraint->premise_occurrences[1] = request_argument;
			constraint->premise_occurrences[2] = request_continuation;
			constraint->premise_occurrence_count = 3;
			const int request_roles[3] = {
				PROTOTYPE_TERM_CHILD_REQUEST_OPERATION,
				PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT,
				PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION
			};
			for (uint32_t premise = 0; premise < 3; ++premise) {
				uint32_t premise_projection;
				if (operation_solver_projected_child(
						ctx, owner_typed_projection,
						request_roles[premise], 0,
						&premise_projection
					) != 0 || operation_solver_add_projected_constraint_dependency(
						ctx, premise_projection, lifecycle_id
					) != 0) {
					return -1;
				}
			}
			continue;
		}
		uint32_t computation_occurrence;
		uint32_t return_operation;
		uint32_t premise_count = 2 + 2 * operation->fold_clause_count;
		if (compile_occurrence_child(
				ctx, i, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, i, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&return_operation
			) != 0 || premise_count > 64 ||
			(operation->fold_clause_count != 0 &&
			 ((size_t)operation->first_fold_clause + operation->fold_clause_count >
				ctx->metadata->typed_occurrences.fold_clause_count))) {
			return -1;
		}
		constraint->premise_occurrences[0] = computation_occurrence;
		constraint->premise_occurrences[1] = return_operation;
		if (ctx->metadata->typed_occurrences.occurrences[return_operation].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_LAMBDA && compile_occurrence_child(
				ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
				&constraint->return_body_occurrence_id
			) != 0) {
			return -1;
		}
		for (uint32_t clause = 0; clause < operation->fold_clause_count; ++clause) {
			uint32_t clause_operation;
			uint32_t clause_lambda;
			uint32_t clause_body;
			if (compile_fold_clause_occurrences(
					ctx, i, clause, &clause_operation, &clause_lambda,
					&clause_body
				) != 0) {
				return -1;
			}
			constraint->premise_occurrences[2 + 2 * clause] =
				clause_operation;
			constraint->premise_occurrences[3 + 2 * clause] =
				clause_lambda;
			(void)clause_body;
		}
		constraint->premise_occurrence_count = premise_count;
		uint32_t premise_projection;
		if (operation_solver_projected_child(
				ctx, owner_typed_projection,
				PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&premise_projection
			) != 0 || operation_solver_add_projected_constraint_dependency(
				ctx, premise_projection, lifecycle_id
			) != 0 || operation_solver_projected_child(
				ctx, owner_typed_projection,
				PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&premise_projection
			) != 0 || operation_solver_add_projected_constraint_dependency(
				ctx, premise_projection, lifecycle_id
			) != 0) {
			return -1;
		}
		for (uint32_t clause = 0; clause < operation->fold_clause_count; ++clause) {
			const int clause_roles[2] = {
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION,
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY
			};
			for (uint32_t premise = 0; premise < 2; ++premise) {
				if (operation_solver_projected_child(
						ctx, owner_typed_projection,
						clause_roles[premise], clause,
						&premise_projection
					) != 0 || operation_solver_add_projected_constraint_dependency(
						ctx, premise_projection, lifecycle_id
					) != 0) {
					return -1;
				}
			}
		}
	}
	return operation_constraint_db_validate(ctx);
}

static int operation_solver_generate_computation_constraints(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	ctx->metadata->computation_constraint_generation_pass_count++;
	struct prototype_typing_constraint_db_mark mark =
		prototype_typing_constraint_db_mark(&ctx->typing->constraints);
	int status = operation_solver_generate_computation_constraints_impl(ctx);
	if (status != 0 && prototype_typing_constraint_db_rollback(
			&ctx->typing->constraints, mark
		) != 0) {
		return -1;
	}
	return status;
}

static int operation_solver_bind_computation_constraint_solutions(
	struct compile_context* ctx,
	int* p_changed,
	int publish_occurrences
) {
	if (!ctx || !p_changed) {
		return -1;
	}
	if (ctx->typing->constraints.topology_sealed) {
		ctx->metadata->proof_solver_call_by_domain[
			PROTOTYPE_COMPILE_SOLUTION_COMPUTATION
		]++;
		return -1;
	}
	/* Requests refine the computations consumed by enclosing folds. Apply their
	 * solutions first so the fold snapshot check below observes those
	 * refinements independently of source/storage order. */
	for (int pass = 0; pass < 2; ++pass) {
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		const struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (i >= ctx->computation_constraint_result_capacity) {
			fprintf(stderr, "computation binding result capacity id=%zu capacity=%zu\n",
				i, ctx->computation_constraint_result_capacity);
			return -1;
		}
		const struct prototype_judgement_computation_constraint_result* result =
			&ctx->computation_constraint_results[i];
		const struct prototype_judgement_computation_constraint_input* input =
			&ctx->computation_constraint_inputs[i];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t lifecycle_id = constraint->occurrence_id < 4096 ?
			ctx->typing->constraints.computation_constraint_for_occurrence[
				constraint->occurrence_id
			] : PROTOTYPE_INVALID_ID;
		if (lifecycle_id == PROTOTYPE_INVALID_ID ||
			lifecycle_id >= ctx->typing->constraints.constraint_count) {
			fprintf(stderr,
				"computation binding lifecycle missing delta=%zu total-delta=%zu count=%u total=%u\n",
				i, ctx->judgement_delta.computation_constraint_count,
				prototype_typing_constraint_count_in_domain(
					&ctx->typing->constraints,
					OPERATION_CONSTRAINT_DOMAIN_COMPUTATION
				),
				ctx->typing->constraints.constraint_count);
			return -1;
		}
		struct prototype_typing_constraint* lifecycle =
			&ctx->typing->constraints.constraints[lifecycle_id];
		struct prototype_typing_constraint_solution* lifecycle_solution =
			&ctx->typing->constraints.solutions[lifecycle_id];
		if (lifecycle->domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			lifecycle->payload.computation.judgement_delta_constraint_id != i) {
			return -1;
		}
		if (result->solved_classifier == PROTOTYPE_INVALID_ID &&
			lifecycle_solution->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			continue;
		}
		int state = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CONSTRAINT_STATE_PENDING :
			OPERATION_CONSTRAINT_STATE_SOLVED;
		int reason = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
			OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
		if (prototype_typing_constraint_transition(
				&ctx->typing->constraints, lifecycle_id, state, reason
			) != 0) {
			fprintf(stderr,
				"computation binding lifecycle transition delta=%zu current=%d target=%d\n",
				i, lifecycle_solution->state, state);
			return -1;
		}
		if (!publish_occurrences) {
			continue;
		}
		int is_request = constraint->kind ==
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		if ((pass == 0) != is_request) {
			continue;
		}
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID ||
			result->solved_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		/* Solutions are derived from the premise-classifier snapshot populated
		 * before this binding pass. Earlier solutions in this same pass can
		 * refine an operation used by a later constraint. Do not publish the
		 * dependent stale solution; the next fixed-point round will refresh and
		 * solve that constraint from the refined premise. */
		int stale = 0;
		for (uint32_t premise = 0;
			premise < constraint->premise_occurrence_count;
			++premise) {
			uint32_t operation_id = constraint->premise_occurrences[premise];
			if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				fprintf(stderr,
					"computation binding premise range delta=%zu premise=%u occurrence=%u count=%zu\n",
					i, premise, operation_id,
					ctx->metadata->typed_occurrences.occurrence_count);
				return -1;
			}
			uint32_t premise_projection = operation_solver_publication_projection(
				ctx, operation_id
			);
			uint32_t current = premise_projection == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID : operation_solver_classifier_for_projection(
					ctx, premise_projection
				);
			uint32_t snapshot = input->premise_classifiers[premise];
			if (current == snapshot) {
				continue;
			}
			if (current == PROTOTYPE_INVALID_ID ||
				snapshot == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, current, snapshot
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				stale = 1;
				break;
			}
		}
		if (stale) {
			continue;
		}
		struct prototype_core_inspect_counts core_counts;
		uint32_t owner_projection = operation_solver_publication_projection(
			ctx, constraint->occurrence_id
		);
		if (typing_core_inspect_counts(
				ctx->core_requests, &core_counts, NULL
			) != 0) {
			return -1;
		}
		if (constraint->occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count ||
			owner_projection == PROTOTYPE_INVALID_ID ||
			result->solved_classifier >= core_counts.term_count ||
			operation_solver_publish_projection(
				ctx,
				owner_projection,
				result->solved_classifier,
				p_changed
			) != 0) {
			fprintf(stderr,
				"computation binding publication failed delta=%zu occurrence=%u result=%u occurrence-count=%zu term-count=%zu\n",
				i, constraint->occurrence_id, result->solved_classifier,
				ctx->metadata->typed_occurrences.occurrence_count,
				core_counts.term_count);
			return -1;
		}
	}
	}
	return 0;
}

static int operation_solver_prepare_computation_constraint_inputs(
	struct compile_context* ctx,
	struct prototype_judgement_computation_constraint_input* inputs,
	size_t input_capacity
) {
	if (!ctx || !ctx->metadata || !inputs) {
		return -1;
	}
	int trace = getenv("A_PROGRAM_COMPUTATION_INPUT_TRACE") != NULL;
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		const struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (i >= input_capacity) {
			return -1;
		}
		struct prototype_judgement_computation_constraint_input* input =
			&inputs[i];
		memset(input, 0, sizeof(*input));
		input->state = PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		input->context_id = PROTOTYPE_INVALID_ID;
		input->subject = PROTOTYPE_INVALID_ID;
		input->computation = PROTOTYPE_INVALID_ID;
		input->continuation = PROTOTYPE_INVALID_ID;
		input->argument = PROTOTYPE_INVALID_ID;
		input->application = PROTOTYPE_INVALID_ID;
		input->projected_classifier = PROTOTYPE_INVALID_ID;
		for (uint32_t premise = 0;
			premise < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES;
			++premise) {
			input->premise_contexts[premise] = PROTOTYPE_INVALID_ID;
			input->premise_classifiers[premise] = PROTOTYPE_INVALID_ID;
			input->premise_states[premise] =
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			if (trace) {
				fprintf(stderr,
					"computation input failed constraint=%zu stage=owner-range "
					"owner=%u count=%zu\n",
					i, constraint->occurrence_id,
					ctx->metadata->typed_occurrences.occurrence_count);
			}
			return -1;
		}
		uint32_t owner_context;
		uint32_t owner_substitution;
		uint32_t owner_subject;
		int owner_status = operation_occurrence_materialized_typing_status(
				ctx,
				constraint->occurrence_id,
				&owner_context,
				&owner_substitution,
				&owner_subject,
				&input->projected_classifier
			);
		if (owner_status < 0) {
			if (trace) {
				fprintf(stderr,
					"computation input failed constraint=%zu stage=owner-typing "
					"owner=%u status=%d\n",
					i, constraint->occurrence_id, owner_status);
			}
			return -1;
		}
		if (owner_status > 0) {
			continue;
		}
		(void)owner_substitution;
		input->context_id = owner_context;
		input->subject = owner_subject;
		struct prototype_term subject;
		if (typing_core_inspect_term(
				ctx->core_requests, input->subject, &subject
			) != 0) {
			if (trace) {
				fprintf(stderr,
					"computation input failed constraint=%zu stage=subject "
					"owner=%u subject=%u\n",
					i, constraint->occurrence_id, input->subject);
			}
			return -1;
		}
		if (constraint->kind ==
				PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST &&
			subject.tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
			input->computation = subject.as.operation_request.operation;
			input->continuation = subject.as.operation_request.continuation;
			input->argument = subject.as.operation_request.argument;
			struct prototype_core_formation_request request = {
				.kind = PROTOTYPE_CORE_FORM_APP,
				.as.app = {
					.function = input->computation,
					.argument = input->argument
				}
			};
			struct prototype_core_formation_response response = { 0 };
			if (prototype_core_request_form(
					ctx->core_requests, &request, NULL, 0, &response
				) != 0) {
				return -1;
			}
			input->application = response.term_id;
		} else if (constraint->kind ==
				PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD &&
			subject.tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
			input->computation = subject.as.computation_fold.computation;
			input->continuation = subject.as.computation_fold.return_clause;
		} else {
			if (trace) {
				fprintf(stderr,
					"computation input failed constraint=%zu stage=owner-shape "
					"owner=%u kind=%d subject=%u tag=%d\n",
					i, constraint->occurrence_id, constraint->kind,
					input->subject, subject.tag);
			}
			return -1;
		}
		input->state = PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED;
		if (constraint->premise_occurrence_count > 64) {
			return -1;
		}
		for (uint32_t premise = 0;
			premise < constraint->premise_occurrence_count;
			++premise) {
			uint32_t operation_id = constraint->premise_occurrences[premise];
			if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			uint32_t premise_context;
			uint32_t premise_substitution;
			uint32_t premise_subject;
			uint32_t classifier;
			int premise_status = operation_occurrence_materialized_typing_status(
					ctx,
					operation_id,
					&premise_context,
					&premise_substitution,
					&premise_subject,
					&classifier
				);
			if (premise_status != 0) {
				if (trace) {
					fprintf(stderr,
						"computation input failed constraint=%zu stage=premise-typing "
						"owner=%u premise=%u operation=%u status=%d\n",
						i, constraint->occurrence_id, premise, operation_id,
						premise_status);
				}
				return -1;
			}
			(void)premise_substitution;
			input->premise_classifiers[premise] = classifier;
			memset(
				&input->premise_evidence[premise],
				0,
				sizeof(input->premise_evidence[premise])
			);
			input->premise_contexts[premise] = premise_context;
			if (classifier == PROTOTYPE_INVALID_ID) {
				input->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
				continue;
			}
			int local = operation_classifier_contains_unowned_effect_row(
				ctx, operation_id, classifier
			);
			if (local < 0) {
				if (trace) {
					fprintf(stderr,
						"computation input failed constraint=%zu stage=premise-row "
						"owner=%u premise=%u operation=%u classifier=%u\n",
						i, constraint->occurrence_id, premise, operation_id,
						classifier);
				}
				return -1;
			}
			if (local) {
				input->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL;
				continue;
			}
			int evidence_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				operation_id,
				owner_context,
				premise_subject,
				classifier,
				&input->premise_evidence[premise]
			);
			if (evidence_status < 0) {
				if (trace) {
					fprintf(stderr,
						"computation input failed constraint=%zu stage=premise-evidence "
						"owner=%u premise=%u operation=%u classifier=%u context=%u\n",
						i, constraint->occurrence_id, premise, operation_id,
						classifier, owner_context);
				}
				return -1;
			}
			input->premise_states[premise] = evidence_status == 0 ?
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED :
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
	}
	return 0;
}
