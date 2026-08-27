static int compile_phase_infer_general_classifiers(
	struct compile_context* ctx,
	int require_complete
) {
	if (!ctx) {
		return -1;
	}
	return operation_solver_solve(ctx, require_complete);
}

static size_t count_classified_operations(const struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	size_t count = 0;
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		if (operation_available_classifier(ctx, i) != PROTOTYPE_INVALID_ID) {
			count++;
		}
	}
	return count;
}

static int operation_solver_match_classifier_parts(
	struct compile_context* ctx,
	uint32_t classifier,
	int* p_returns_computation,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
);

static int operation_solver_branch_subtree_reaches(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
);

static void operation_solver_invalidate_reindex_cache(
	struct compile_context* ctx
) {
	if (!ctx) {
		return;
	}
	if (ctx->branch_reachability_valid) {
		memset(
			ctx->branch_reachability_valid,
			0,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY
		);
	}
	ctx->operation_reindex_revision++;
	if (ctx->operation_reindex_revision == 0) {
		memset(
			ctx->operation_reindex_cache,
			0,
			sizeof(ctx->operation_reindex_cache)
		);
		ctx->operation_reindex_revision = 1;
	}
}

static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
) {
	if (!ctx || !p_reindexed || term >= ctx->terms->term_count ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	uint32_t cache_hash = operation_id * UINT32_C(2654435761) ^
		context_id * UINT32_C(2246822519) ^ term * UINT32_C(3266489917);
	uint32_t cache_slot = cache_hash % OPERATION_REINDEX_CACHE_CAPACITY;
	const struct operation_reindex_cache_entry* cached =
		&ctx->operation_reindex_cache[cache_slot];
	if (cached->occupied && cached->occurrence_id == operation_id &&
		cached->context_id == context_id && cached->term == term &&
		cached->dependency_signature == ctx->operation_reindex_revision) {
		*p_reindexed = cached->reindexed;
		return 0;
	}
	uint32_t current = term;
	for (uint32_t match_cursor = ctx->metadata->typed_occurrences.occurrence_count;
		match_cursor > 0;) {
		uint32_t match_id = --match_cursor;
		const struct prototype_typed_occurrence* match_operation =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t scrutinee_operation;
		if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			compile_occurrence_child(
				ctx, match_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&scrutinee_operation
			) != 0 ||
			match_operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[match_operation->core_term].tag !=
				PROTOTYPE_TERM_MATCH) {
			continue;
		}
		for (uint32_t case_index = 0;
			case_index < match_operation->case_count; ++case_index) {
			uint32_t branch_operation;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[
					match_operation->first_case + case_index
				];
			uint32_t context_path[512];
			uint32_t context_path_count;
			int context_path_status = prototype_context_extension_path(
				&ctx->metadata->contexts, operation_case->context_id,
				context_id, context_path, 512, &context_path_count
			);
			int operation_path_status = compile_occurrence_child(
				ctx, match_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
				&branch_operation
			) == 0 ? operation_solver_branch_subtree_reaches(
				ctx, branch_operation, operation_id
			) : -1;
			if (operation_case->constructor_id == PROTOTYPE_INVALID_ID ||
				context_path_status != 0 || operation_path_status == 0) {
				continue;
			}
			if (operation_path_status < 0) {
				return -1;
			}
			uint32_t refinement_substitution = PROTOTYPE_INVALID_ID;
			int refinement_status = PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				const struct prototype_substitution* refinement =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation_case->refinement_substitution
					);
				if (!refinement ||
					refinement->source_context != operation_case->context_id) {
					return -1;
				}
				refinement_substitution = operation_case->refinement_substitution;
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_SOLVED;
			} else {
				struct operation_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, match_operation->first_case + case_index
					);
				if (refinement) {
					refinement_status = refinement->payload.branch_refinement.
						refinement_status;
					refinement_substitution = refinement->payload.
						branch_refinement.substitution;
				}
			}
			if (refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
				continue;
			}
			if (prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts, &ctx->metadata->substitutions,
					current, refinement_substitution, &current
				) != 0) {
				return -1;
			}
		}
	}
	*p_reindexed = current;
	/* TermDB nodes are immutable once interned. Cache against the enclosing
	 * indexed branches that can change this action and overwrite collisions. */
	struct operation_reindex_cache_entry* entry =
		&ctx->operation_reindex_cache[cache_slot];
	entry->occurrence_id = operation_id;
	entry->context_id = context_id;
	entry->term = term;
	entry->reindexed = current;
	entry->dependency_signature = ctx->operation_reindex_revision;
	entry->occupied = 1;
	return 0;
}

static int operation_solver_indexed_branch_endomorphism(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t* p_substitution
) {
	if (!ctx || !p_substitution || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	uint32_t path[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			prototype_context_empty(&ctx->metadata->contexts),
			context_id,
			path,
			512,
			&path_count
		) != 0) {
		return -1;
	}
	uint32_t arguments[512];
	for (uint32_t i = 0; i < path_count; ++i) {
		const struct prototype_context* entry = prototype_context_get(
			&ctx->metadata->contexts, path[i]
		);
		uint32_t variable;
		if (!entry || entry->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(ctx->terms, entry->binding_id, &variable) != 0 ||
			operation_solver_reindex_in_enclosing_indexed_branches(
				ctx, operation_id, context_id, variable, &arguments[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_substitution_from_terms(
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		context_id,
		context_id,
		arguments,
		path_count,
		p_substitution
	);
}

static int operation_solver_branch_subtree_index_at_depth(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t operation_id,
	uint8_t* visited,
	uint64_t* reachable
) {
	if (!graph || !terms || !visited || !reachable ||
		operation_id >= graph->occurrence_count) {
		return -1;
	}
	if (visited[operation_id]) {
		return 0;
	}
	visited[operation_id] = 1;
	reachable[operation_id / 64] |= UINT64_C(1) << (operation_id % 64);
	const struct prototype_typed_occurrence* operation =
		&graph->occurrences[operation_id];
	/* A reference occurrence belongs to the branch, but its wrapped definition
	 * does not. Pulling the definition body into the branch Context would mutate
	 * a separately named source occurrence. Expected-type wrappers, by contrast,
	 * are local elaboration nodes and remain part of the branch subtree. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
		return 0;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (operation_solver_branch_subtree_index_at_depth(
				graph, terms, operation->wrapped_occurrence, visited, reachable
			) != 0) {
			return -1;
		}
	}
	uint32_t child_count;
	if (prototype_typed_occurrence_core_child_count(
			graph, terms, operation_id, &child_count
		) != 0) {
		return -1;
	}
	for (uint32_t child_index = 0; child_index < child_count; ++child_index) {
		uint32_t child;
		if (prototype_typed_occurrence_core_child(
				graph, terms, operation_id, child_index, &child
			) != 0) {
			return -1;
		}
		if (operation_solver_branch_subtree_index_at_depth(
				graph, terms, child, visited, reachable
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_branch_subtree_reaches(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!ctx || !ctx->metadata || !ctx->terms ||
		!ctx->branch_reachability_bits || !ctx->branch_reachability_valid ||
		!ctx->branch_reachability_occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		&ctx->metadata->typed_occurrences;
	if (graph->occurrence_count >
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY ||
		root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
		return -1;
	}
	if (*ctx->branch_reachability_occurrence_count != graph->occurrence_count) {
		memset(
			ctx->branch_reachability_valid,
			0,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY
		);
		*ctx->branch_reachability_occurrence_count = graph->occurrence_count;
	}
	if (!ctx->branch_reachability_valid[root_operation]) {
		uint8_t visited[PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY];
		uint64_t* reachable = ctx->branch_reachability_bits[root_operation];
		memset(visited, 0, sizeof(visited));
		memset(
			reachable,
			0,
			(PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY / 64) *
				sizeof(*reachable)
		);
		if (operation_solver_branch_subtree_index_at_depth(
				graph, ctx->terms, root_operation, visited, reachable
			) != 0) {
			return -1;
		}
		ctx->branch_reachability_valid[root_operation] = 1;
	}
	return (ctx->branch_reachability_bits[root_operation][target_operation / 64] &
		(UINT64_C(1) << (target_operation % 64))) != 0;
}

static int operation_solver_occurrence_is_in_impossible_branch(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		&ctx->metadata->typed_occurrences;
	for (uint32_t match_id = 0; match_id < graph->occurrence_count; ++match_id) {
		const struct prototype_typed_occurrence* match = &graph->occurrences[match_id];
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			match->first_case + match->case_count > graph->case_count) {
			continue;
		}
		for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
			const struct prototype_typed_occurrence_match_case* operation_case =
				&graph->cases[match->first_case + case_index];
			if (operation_case->refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			uint32_t branch_operation;
			if (prototype_typed_occurrence_graph_child(
					graph,
					match_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index,
					&branch_operation
				) != 0) {
				return -1;
			}
			int reaches = operation_solver_branch_subtree_reaches(
				ctx, branch_operation, occurrence_id
			);
			if (reaches != 0) {
				return reaches;
			}
		}
	}
	return 0;
}

static int operation_solver_pullback_branch_operations(
	struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t source_context,
	uint32_t refined_context,
	uint32_t refinement_substitution
) {
	if (!ctx || !ctx->metadata ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences(ctx->metadata);
	uint8_t reachable_operations[graph->occurrence_count];
	uint8_t reachable_cases[graph->case_count == 0 ? 1 : graph->case_count];
	for (uint32_t operation_id = 0;
		operation_id < graph->occurrence_count; ++operation_id) {
		int reaches = operation_solver_branch_subtree_reaches(
			ctx, root_operation, operation_id
		);
		if (reaches < 0) {
			fprintf(stderr,
				"branch pullback reachability invalid root=%u operation=%u\n",
				root_operation, operation_id);
			return -1;
		}
		reachable_operations[operation_id] = reaches > 0;
	}
	for (uint32_t case_id = 0; case_id < graph->case_count; ++case_id) {
		uint32_t owner_match = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < graph->occurrence_count; ++operation_id) {
			const struct prototype_typed_occurrence* candidate =
				&graph->occurrences[operation_id];
			if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				case_id >= candidate->first_case &&
				case_id - candidate->first_case < candidate->case_count) {
				owner_match = operation_id;
				break;
			}
		}
		if (owner_match == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		reachable_cases[case_id] = reachable_operations[owner_match];
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count; ++operation_id) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t path[256];
		uint32_t path_count;
		if (!reachable_operations[operation_id] || prototype_context_extension_path(
				&ctx->metadata->contexts, source_context,
				operation->context_id, path, 256, &path_count
			) != 0) {
			continue;
		}
		uint32_t target_context = refined_context;
		uint32_t action = refinement_substitution;
		if (operation->context_id != source_context &&
			prototype_context_pullback_occurrence_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				source_context,
				operation->context_id,
				refinement_substitution,
				&target_context,
				&action
			) != 0) {
			fprintf(stderr,
				"branch pullback telescope failed root=%u operation=%u source=%u operation-context=%u substitution=%u\n",
				root_operation, operation_id, source_context,
				operation->context_id, refinement_substitution);
			return -1;
		}
		uint32_t classifier = operation_available_classifier(ctx, operation_id);
		uint32_t known_classifier = ctx->classifier_seeds[operation_id].classifier;
		uint32_t binder_classifier = operation_solver_binder_classifier(
			ctx, operation_id
		);
		int classifier_is_structural_seed =
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS;
		if (!classifier_is_structural_seed && operation->tag !=
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			/* A derived occurrence has no independent generation fact that may
			 * survive a Context action. */
			known_classifier = PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			classifier == PROTOTYPE_INVALID_ID &&
			known_classifier == PROTOTYPE_INVALID_ID &&
			operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_VAR) {
			uint32_t binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					operation->context_id,
					ctx->terms->terms[
						operation->core_term
					].as.var.binding_id,
					&binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* binding = prototype_context_get(
				&ctx->metadata->contexts, binding_context
			);
			if (!binding) {
				return -1;
			}
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding);
			if (binding_classifier != PROTOTYPE_INVALID_ID) {
				classifier = binding_classifier;
				known_classifier = classifier;
			}
		}
		uint32_t source_classifier = known_classifier != PROTOTYPE_INVALID_ID ?
			known_classifier : classifier_is_structural_seed ?
				classifier : PROTOTYPE_INVALID_ID;
		int has_source_classifier =
			source_classifier != PROTOTYPE_INVALID_ID;
		uint32_t projected_core = operation->core_term;
#define REINDEX_OPERATION_FIELD(field) \
	do { \
		if ((field) != PROTOTYPE_INVALID_ID && prototype_term_reindex( \
				ctx->terms, ctx->type_declarations, \
				&ctx->metadata->contexts, &ctx->metadata->substitutions, \
				(field), action, &(field) \
			) != 0) { \
			fprintf(stderr, \
				"branch pullback term reindex failed root=%u operation=%u field=%s term=%u action=%u source=%u target=%u\n", \
				root_operation, operation_id, #field, (field), action, \
				source_context, target_context); \
			return -1; \
		} \
	} while (0)
		REINDEX_OPERATION_FIELD(classifier);
		REINDEX_OPERATION_FIELD(known_classifier);
		REINDEX_OPERATION_FIELD(binder_classifier);
		REINDEX_OPERATION_FIELD(projected_core);
#undef REINDEX_OPERATION_FIELD
		for (uint32_t check_id = 0;
			check_id < ctx->pending_ascription_check_count;
			++check_id) {
			struct pending_ascription_check* check =
				&ctx->pending_ascription_checks[check_id];
			if (check->operation != operation_id) {
				continue;
			}
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					check->subject,
					action,
					&check->subject
				) != 0 || prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					check->expected_classifier,
					action,
					&check->expected_classifier
				) != 0) {
				return -1;
			}
		}
		int rebuilt_in_target_context = 0;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
			int saturated;
			int constructor_status =
				prototype_judgement_constructor_spine_classifier(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					target_context,
					projected_core,
					PROTOTYPE_INVALID_ID,
					NULL,
					0,
					&classifier,
					&saturated
				);
			if (constructor_status !=
					PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID) {
				return -1;
			}
			(void)saturated;
			known_classifier = classifier;
			rebuilt_in_target_context = 1;
		}
		if (rebuilt_in_target_context) {
			operation->source_core_term = PROTOTYPE_INVALID_ID;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
			action = PROTOTYPE_INVALID_ID;
		} else if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
			uint32_t composed;
			if (prototype_substitution_compose(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					operation->context_action_substitution,
					action,
					&composed
				) != 0) {
				fprintf(stderr,
					"branch pullback action composition failed root=%u operation=%u existing=%u action=%u\n",
					root_operation, operation_id,
					operation->context_action_substitution, action);
				return -1;
			}
			action = composed;
		} else if (has_source_classifier) {
			operation->source_core_term = operation->core_term;
			operation->source_classifier = source_classifier;
		} else {
			/* Preserve source topology so the enclosing Core node can validate its
			 * child projection. No source typing derivation existed yet, so the
			 * final classifier is established directly in target_context. */
			operation->source_core_term = operation->core_term;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
		}
		if (!rebuilt_in_target_context && !classifier_is_structural_seed) {
			/* APP/Lambda/Match and CBPV boundary classifiers are consequences of
			 * their child occurrences. Reindexing a provisional parent preserves
			 * stale child choices, so retain only an authoritative hint and let the
			 * ordinary constraints rebuild every other derived classifier. */
			classifier = known_classifier;
		}
		operation->context_id = target_context;
		operation->context_action_substitution = action;
		operation->core_term = projected_core;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			operation_solver_binder_classifier(ctx, operation_id) !=
				binder_classifier) {
			operation_context_binding_dependency_changed(
				ctx, operation->binding_id
			);
		}
		/* Branch pullback is one atomic Context action over the reachable graph,
		 * not a sequence of ordinary meta refinements. Publishing STALE cells here
		 * cascades invalidation through a graph whose remaining occurrences still
		 * belong to the old Context. Install the reindexed structural seeds as the
		 * new invocation state; the caller regenerates and queues all constraints
		 * after the complete pullback. */
		if (operation_type_metas_reset_after_context_action(
				ctx, operation_id, known_classifier, classifier, binder_classifier
			) != 0) {
			return -1;
		}
	}
	for (uint32_t case_id = 0;
		case_id < ctx->metadata->typed_occurrences.case_count; ++case_id) {
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		uint32_t owner_match = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->typed_occurrences.occurrence_count; ++operation_id) {
			const struct prototype_typed_occurrence* candidate =
				&ctx->metadata->typed_occurrences.occurrences[operation_id];
			if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				case_id >= candidate->first_case &&
				case_id - candidate->first_case < candidate->case_count) {
				owner_match = operation_id;
				break;
			}
		}
		uint32_t path[256];
		uint32_t path_count;
		if (owner_match == PROTOTYPE_INVALID_ID || !reachable_cases[case_id] ||
			prototype_context_extension_path(
				&ctx->metadata->contexts, source_context,
				operation_case->context_id, path, 256, &path_count
			) != 0) {
			continue;
		}
		uint32_t target_context = refined_context;
		uint32_t action = refinement_substitution;
		if (operation_case->context_id != source_context &&
			prototype_context_pullback_occurrence_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				source_context,
				operation_case->context_id,
				refinement_substitution,
				&target_context,
				&action
			) != 0) {
			return -1;
		}
		uint32_t owner = operation_case->constructor_owner;
		if (owner != PROTOTYPE_INVALID_ID && prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				owner, action, &owner
			) != 0) {
			return -1;
		}
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			uint32_t composed_refinement;
			if (operation_case->refinement_substitution ==
					PROTOTYPE_INVALID_ID || prototype_substitution_compose(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					operation_case->refinement_substitution,
					action,
					&composed_refinement
				) != 0) {
				return -1;
			}
			operation_case->refinement_substitution = composed_refinement;
		}
		operation_case->context_id = target_context;
		operation_case->constructor_owner = owner;
	}
	/* An outer pullback changes a whole nested branch subtree at once. Rebuild
	 * every mutable branch constraint from its occurrence/case authority so no
	 * payload observes a partially transformed graph. */
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->source_occurrence >= graph->occurrence_count ||
			match_id >= graph->occurrence_count ||
			constraint->payload.branch_refinement.case_index >=
				graph->occurrences[match_id].case_count) {
			return -1;
		}
		uint32_t case_id = graph->occurrences[match_id].first_case +
			constraint->payload.branch_refinement.case_index;
		if (case_id >= graph->case_count) {
			return -1;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&graph->cases[case_id];
		constraint->context_id =
			graph->occurrences[constraint->source_occurrence].context_id;
		if (!prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			continue;
		}
		const struct prototype_substitution* branch_action =
			prototype_substitution_get(
				&ctx->metadata->substitutions,
				operation_case->refinement_substitution
			);
		if (!branch_action || branch_action->source_context !=
				operation_case->context_id) {
			return -1;
		}
		constraint->payload.branch_refinement.source_context =
			branch_action->target_context;
		constraint->payload.branch_refinement.refined_context =
			branch_action->source_context;
		constraint->payload.branch_refinement.substitution =
			operation_case->refinement_substitution;
		constraint->payload.branch_refinement.refinement_status =
			PROTOTYPE_INDEX_REFINEMENT_SOLVED;
	}
	ctx->classifier_solver.solution_revision++;
	operation_solver_invalidate_reindex_cache(ctx);
	return 0;
}

static int operation_solver_occurrence_waits_for_branch_action(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		uint32_t case_id = match_id <
			ctx->metadata->typed_occurrences.occurrence_count ?
			ctx->metadata->typed_occurrences.occurrences[match_id].first_case +
				constraint->payload.branch_refinement.case_index :
			PROTOTYPE_INVALID_ID;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			case_id >= ctx->metadata->typed_occurrences.case_count) {
			continue;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		if (prototype_typed_occurrence_match_case_is_admitted(operation_case) ||
			operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			continue;
		}
		int reaches = operation_solver_branch_subtree_reaches(
			ctx,
			constraint->source_occurrence,
			occurrence_id
		);
		if (reaches < 0) {
			return -1;
		}
		if (reaches > 0) {
			return 1;
		}
	}
	return 0;
}

static struct operation_constraint* operation_solver_branch_refinement_constraint(
	struct compile_context* ctx,
	uint32_t operation_case_id
) {
	if (!ctx || !ctx->metadata || operation_case_id >=
		ctx->metadata->typed_occurrences.case_count) {
		return NULL;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT &&
			match_id < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[match_id].first_case +
				constraint->payload.branch_refinement.case_index ==
				operation_case_id) {
			return constraint;
		}
	}
	return NULL;
}

static int operation_solver_waits_for_enclosing_branch_refinement(
	const struct compile_context* ctx,
	uint32_t match_id
) {
	if (!ctx || !ctx->metadata || match_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* enclosing =
			&ctx->constraints.constraints[id];
		uint32_t enclosing_match =
			enclosing->payload.branch_refinement.match_occurrence;
		if (enclosing->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			enclosing_match == match_id || enclosing_match >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			continue;
		}
		const struct prototype_typed_occurrence* owner =
			&ctx->metadata->typed_occurrences.occurrences[enclosing_match];
		uint32_t case_id = owner->first_case +
			enclosing->payload.branch_refinement.case_index;
		if (owner->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_id >= ctx->metadata->typed_occurrences.case_count) {
			return -1;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		if (prototype_typed_occurrence_match_case_is_admitted(operation_case) ||
			operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			continue;
		}
		int reaches = operation_solver_branch_subtree_reaches(
			ctx,
			enclosing->source_occurrence,
			match_id
		);
		if (reaches < 0) {
			return -1;
		}
		if (reaches > 0) {
			/* Refinement substitutions compose from outer Contexts inward.
			 * Solving the nested equation first would leave its case action
			 * rooted in the pre-refinement telescope. */
			return 1;
		}
	}
	return 0;
}

static int operation_solver_match_motive_is_constant(
	const struct compile_context* ctx,
	uint32_t match_id
) {
	if (!ctx || !ctx->metadata || match_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t motive = ctx->classifier_solver.motive_solutions[match_id].motive;
	if (motive == PROTOTYPE_INVALID_ID) {
		/* A constant candidate is the solver's branch equation M(_) = T.
		 * It is enough to show that index refinement is irrelevant; motive
		 * materialization still validates every reachable branch against T. */
		return ctx->classifier_solver.motive_solutions[match_id].constant_candidate !=
			PROTOTYPE_INVALID_ID;
	}
	int saw_lambda = 0;
	while (motive < ctx->terms->term_count &&
		ctx->terms->terms[motive].tag == PROTOTYPE_TERM_LAMBDA) {
		const struct prototype_term* lambda = &ctx->terms->terms[motive];
		if (prototype_term_contains_free_binding(
				ctx->terms, lambda->as.lambda.body, lambda->as.lambda.binding_id
			)) {
			return 0;
		}
		saw_lambda = 1;
		motive = lambda->as.lambda.body;
	}
	return saw_lambda && motive < ctx->terms->term_count;
}

static int operation_solver_solve_branch_refinement_constraints(
	struct compile_context* ctx,
	int* p_graph_changed
) {
	if (!ctx || !ctx->metadata || !p_graph_changed) {
		return -1;
	}
	*p_graph_changed = 0;
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		uint32_t scrutinee_id =
			constraint->payload.branch_refinement.scrutinee_occurrence;
		uint32_t case_index =
			constraint->payload.branch_refinement.case_index;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			match_id >= ctx->metadata->typed_occurrences.occurrence_count ||
			scrutinee_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "branch refinement constraint header invalid id=%u\n", id);
			return -1;
		}
		struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t operation_case_id = match->first_case + case_index;
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_index >= match->case_count || operation_case_id >=
				ctx->metadata->typed_occurrences.case_count ||
			match->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
			fprintf(stderr,
				"branch refinement Match header invalid id=%u match=%u case=%u\n",
				id, match_id, case_index);
			return -1;
		}
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[operation_case_id];
		if (prototype_typed_occurrence_match_case_is_admitted(operation_case)) {
			continue;
		}
		if (operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int waits_for_enclosing =
			operation_solver_waits_for_enclosing_branch_refinement(ctx, match_id);
		if (waits_for_enclosing < 0) {
			return -1;
		}
		if (waits_for_enclosing > 0) {
			continue;
		}
		uint32_t scrutinee_classifier = operation_solver_classifier(
			ctx, scrutinee_id
		);
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t term_case_id =
			ctx->terms->terms[match->core_term].as.match.first_case + case_index;
		if (term_case_id >= ctx->terms->case_count) {
			fprintf(stderr, "branch refinement term case invalid id=%u term-case=%u\n",
				id, term_case_id);
			return -1;
		}
		const struct prototype_match_case* term_case =
			&ctx->terms->cases[term_case_id];
		struct prototype_case_binder occurrence_binders[64];
		if (term_case->binder_count > 64 || term_case->binder_count !=
			operation_case->binder_count) {
			fprintf(stderr,
				"branch refinement binder count invalid id=%u term=%u occurrence=%u\n",
				id, term_case->binder_count, operation_case->binder_count);
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < term_case->binder_count;
			++binder_index) {
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					operation_case->context_id,
					operation_case->binder_ids[binder_index]
				)) {
				fprintf(stderr,
					"branch refinement binder scope invalid id=%u binder=%u context=%u\n",
					id, operation_case->binder_ids[binder_index],
					operation_case->context_id);
				return -1;
			}
			occurrence_binders[binder_index] = ctx->terms->case_binders[
				term_case->first_binder + binder_index
			];
			occurrence_binders[binder_index].binding_id =
				operation_case->binder_ids[binder_index];
		}
		uint32_t refined_context = PROTOTYPE_INVALID_ID;
		uint32_t substitution = PROTOTYPE_INVALID_ID;
		uint32_t constructor_term = PROTOTYPE_INVALID_ID;
		uint32_t residual_pattern = PROTOTYPE_INVALID_ID;
		uint32_t residual_value = PROTOTYPE_INVALID_ID;
		int status = prototype_judgement_source_match_branch_refinement(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			match->context_id,
			ctx->metadata->typed_occurrences.occurrences[scrutinee_id].core_term,
			ctx->metadata->typed_occurrences.occurrences[scrutinee_id].binding_id,
			scrutinee_classifier,
			operation_case->constructor_id,
			operation_case->context_id,
			occurrence_binders,
			term_case->binder_count,
			&refined_context,
			&substitution,
			&constructor_term,
			&residual_pattern,
			&residual_value
		);
		if (constraint->payload.branch_refinement.refinement_status != status ||
			constraint->payload.branch_refinement.refined_context != refined_context ||
			constraint->payload.branch_refinement.substitution != substitution) {
			operation_solver_invalidate_reindex_cache(ctx);
		}
		constraint->payload.branch_refinement.refinement_status = status;
		constraint->payload.branch_refinement.refined_context = refined_context;
		constraint->payload.branch_refinement.substitution = substitution;
		constraint->payload.branch_refinement.constructor_term = constructor_term;
		constraint->payload.branch_refinement.residual_pattern = residual_pattern;
		constraint->payload.branch_refinement.residual_value = residual_value;
		if (status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
			fprintf(stderr,
				"branch refinement invalid constraint=%u match=%u case=%u "
				"scrutinee=%u match-context=%u branch-context=%u "
				"scrutinee-classifier=%u constructor=%u ast=%u\n",
				constraint->id, match_id, case_index, scrutinee_id,
				match->context_id, operation_case->context_id,
				scrutinee_classifier, operation_case->constructor_id,
				match->source_ast);
			fprintf(stderr, "branch refinement scrutinee classifier: ");
			prototype_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				scrutinee_classifier
			);
			fprintf(stderr, "\n");
			if (match->source_ast < ctx->asts->node_count) {
				const struct prototype_ast_node* source =
					&ctx->asts->nodes[match->source_ast];
				fprintf(stderr,
					"branch refinement source tag=%d scrutinee-ast=%u cases=%u first-case=%u\n",
					source->tag,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.scrutinee : PROTOTYPE_INVALID_ID,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.case_count : 0,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.first_case : PROTOTYPE_INVALID_ID);
				if (source->tag == PROTOTYPE_AST_MATCH &&
					source->as.match.first_case < ctx->asts->case_count) {
					const struct prototype_ast_match_case* source_case =
						&ctx->asts->cases[source->as.match.first_case];
					fprintf(stderr,
						"branch refinement source case constructor-symbol=%d binders=%u body=%u\n",
						source_case->constructor_symbol_id,
						source_case->binder_count, source_case->body);
				}
			}
			(void)operation_constraint_transition(
				constraint,
				OPERATION_CONSTRAINT_STATE_CONTRADICTION,
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_INVALID
			);
			return -1;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			int constant_motive = operation_solver_match_motive_is_constant(
				ctx, match_id
			);
			if (constant_motive < 0) {
				return -1;
			}
			if (constant_motive > 0) {
				operation_case->refinement_status =
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT;
				operation_case->refinement_substitution = PROTOTYPE_INVALID_ID;
				constraint->payload.branch_refinement.refinement_status =
					PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
				if (operation_constraint_reopen(
						constraint,
						OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT
					) != 0 || operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_SOLVED,
						OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT
					) != 0) {
					return -1;
				}
				*p_graph_changed = 1;
				continue;
			}
			operation_case->refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_RESIDUAL;
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_RESIDUAL,
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_RESIDUAL
				) != 0) {
				return -1;
			}
			continue;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
			operation_case->refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE;
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_SOLVED,
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE
				) != 0) {
				return -1;
			}
			continue;
		}
		if (status != PROTOTYPE_INDEX_REFINEMENT_SOLVED ||
			refined_context == PROTOTYPE_INVALID_ID ||
			substitution == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		const struct prototype_substitution* solved_action =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution
			);
		if (!solved_action || solved_action->source_context != refined_context) {
			return -1;
		}
		uint32_t branch_operation = constraint->source_occurrence;
		uint32_t source_context = operation_case->context_id;
		if (operation_solver_pullback_branch_operations(
				ctx,
				branch_operation,
				source_context,
				refined_context,
				substitution
			) != 0) {
			fprintf(stderr,
				"branch refinement occurrence pullback failed id=%u branch=%u source=%u refined=%u substitution=%u\n",
				id, branch_operation, source_context, refined_context,
				substitution);
			return -1;
		}
		uint32_t refined_constructor_owner = operation_case->constructor_owner;
		if (refined_constructor_owner != PROTOTYPE_INVALID_ID &&
			prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				refined_constructor_owner,
				substitution,
				&refined_constructor_owner
			) != 0) {
			fprintf(stderr,
				"branch refinement owner reindex failed id=%u owner=%u substitution=%u\n",
				id, operation_case->constructor_owner, substitution);
			return -1;
		}
		operation_case->context_id = refined_context;
		operation_case->constructor_owner = refined_constructor_owner;
		operation_case->refinement_status =
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED;
		operation_case->refinement_substitution = substitution;
		constraint->payload.branch_refinement.source_context =
			solved_action->target_context;
		constraint->context_id = refined_context;
		constraint->state = OPERATION_CONSTRAINT_STATE_SOLVED;
		constraint->reason =
			OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED;
		*p_graph_changed = 1;
		/* The pullback changes every nested occurrence Context. Regenerate all
		 * constraint origins before considering another branch. */
		return 0;
	}
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
);

static int operation_solver_validate_guarded_motive_occurrence(
	struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence
);

/* Solve a dependent Match equation from the branch Context morphisms. This
 * is the graph-level counterpart of anti-unifying branch classifiers: every
 * branch must quote to one source term, and the resulting motive must replay
 * to every independently synthesized branch classifier. */
static int operation_usage_solution_view(
	const struct compile_context* ctx,
	uint32_t operation_id,
	struct prototype_usage_vector* p_vector
) {
	if (!ctx || !ctx->metadata || !p_vector ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	return prototype_occurrence_usage_solution_view(
		ctx->operation_usage_solutions,
		ctx->metadata->typed_occurrences.occurrence_count,
		ctx->operation_usage_entries,
		ctx->operation_usage_entry_count,
		operation_id,
		p_vector
	);
}

static int operation_solver_proposition_usage(
	void* context,
	uint32_t operation_id,
	const struct prototype_usage_entry** p_entries,
	uint32_t* p_count
) {
	struct compile_context* ctx = context;
	struct prototype_usage_vector usage;
	if (!ctx || !p_entries || !p_count ||
		operation_usage_solution_view(ctx, operation_id, &usage) != 0 ||
		usage.count > UINT32_MAX) {
		return -1;
	}
	*p_entries = usage.entries;
	*p_count = (uint32_t)usage.count;
	return 0;
}


static int operation_solver_refresh_usage_solutions(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->operation_usage_entries ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* operations =
		prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	size_t entry_count;
	if (prototype_occurrence_usage_solve(
			operations,
			ctx->terms,
			&ctx->metadata->contexts,
			ctx->operation_usage_solutions,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY,
			ctx->operation_usage_entries,
			ctx->operation_usage_entry_capacity,
			&entry_count
		) != 0 || entry_count > UINT32_MAX) {
		return -1;
	}
	ctx->operation_usage_entry_count = (uint32_t)entry_count;
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_occurrence_usage_solution* source =
			&ctx->operation_usage_solutions[operation_id];
		for (uint32_t usage_index = 0;
			usage_index < source->entry_count;
			++usage_index) {
			const struct prototype_usage_entry* entry =
				&ctx->operation_usage_entries[
					source->first_entry + usage_index
				];
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
					entry->binding_id
				)) {
				const struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				fprintf(stderr, "operation usage escapes context operation=%u tag=%d context=%u binding=%u core=%u first-edge=%u edge-count=%u wrapper=%u ih-owner=%u\n",
					operation_id, operation->tag, operation->context_id,
					entry->binding_id, operation->core_term,
					operation->first_edge, operation->edge_count,
					operation->wrapped_occurrence,
					operation->ih_owner_occurrence);
				if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
					uint32_t debug_context = operation->context_id;
					while (debug_context != prototype_context_empty(
						&ctx->metadata->contexts
					)) {
						const struct prototype_context* debug_entry =
							prototype_context_get(
								&ctx->metadata->contexts, debug_context
							);
						if (!debug_entry) {
							break;
						}
						fprintf(stderr, "operation usage context entry=%u binding=%u parent=%u\n",
							debug_context, debug_entry->binding_id,
							debug_entry->parent);
						debug_context = debug_entry->parent;
					}
					for (uint32_t case_index = 0;
						case_index < operation->case_count;
						++case_index) {
						const struct prototype_typed_occurrence_match_case* debug_case =
							&ctx->metadata->typed_occurrences.cases[
								operation->first_case + case_index
							];
						uint32_t debug_body = PROTOTYPE_INVALID_ID;
						(void)compile_occurrence_child(
							ctx, operation_id,
							PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
							&debug_body
						);
						fprintf(stderr, "operation usage case=%u body=%u context=%u binders=%u ast0=%u\n",
							case_index, debug_body,
							debug_case->context_id, debug_case->binder_count,
							debug_case->binder_count ? debug_case->ast_binder_ids[0] :
							PROTOTYPE_INVALID_ID);
					}
					for (uint32_t target_id = 0;
						target_id < ctx->metadata->typed_occurrences.occurrence_count;
						++target_id) {
						const struct prototype_typed_occurrence* target_operation =
							&ctx->metadata->typed_occurrences.occurrences[target_id];
						if (target_operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
							target_operation->binding_id == entry->binding_id) {
							fprintf(stderr, "operation usage binding occurrence operation=%u ast=%u context=%u core=%u\n",
								target_id,
								target_operation->referenced_ast_binder_id,
								target_operation->context_id,
								target_operation->core_term);
						}
					}
				}
				return -1;
			}
		}
	}
	return 0;
}

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		target_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
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
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int status = operation_subtree_contains_operation(
			ctx, operation->wrapped_occurrence, target_operation, visited
		);
		if (status != 0) {
			return status;
		}
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID) {
		return operation->edge_count == 0 ? 0 : -1;
	}
	if (operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count > ctx->metadata->typed_occurrences.edge_count -
			operation->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < operation->edge_count; ++i) {
		const struct prototype_typed_occurrence_edge* edge =
			&ctx->metadata->typed_occurrences.edges[operation->first_edge + i];
		if (edge->child_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int status = operation_subtree_contains_operation(
			ctx, edge->child_occurrence, target_operation, visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
) {
	if (!ctx || !ctx->metadata || !ctx->terms ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t target = 0;
		target < ctx->metadata->typed_occurrences.occurrence_count;
		++target) {
		uint32_t core_term =
			ctx->metadata->typed_occurrences.occurrences[target].core_term;
		if (core_term >= ctx->terms->term_count ||
			ctx->terms->terms[core_term].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, root_operation, target, visited
		);
		if (contains != 0) {
			return contains;
		}
	}
	return 0;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_constraint* equation,
	uint32_t classifier,
	uint32_t* p_classifier_whnf
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		!p_classifier_whnf ||
		equation->payload.classifier.classifier_meta_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t classifier_whnf;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[equation->payload.classifier.classifier_meta_id];
	if (match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH ||
		equation->payload.classifier.goal.motive_case.case_index >= match->case_count ||
		match->first_case + equation->payload.classifier.goal.motive_case.case_index >=
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	const struct prototype_term* core_match = &ctx->terms->terms[match->core_term];
	uint32_t core_case_id = core_match->as.match.first_case +
		equation->payload.classifier.goal.motive_case.case_index;
	if (core_case_id >= ctx->terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* core_case =
		&ctx->terms->cases[core_case_id];
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[
			match->first_case + equation->payload.classifier.goal.motive_case.case_index
		];
	if (core_case->binder_count != operation_case->binder_count) {
		return -1;
	}
	for (uint32_t i = 0; i < core_case->binder_count; ++i) {
		uint32_t canonical_binding = ctx->terms->case_binders[
			core_case->first_binder + i
		].binding_id;
		if (prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, canonical_binding
			) || prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, operation_case->binder_ids[i]
			)) {
			return 1;
		}
	}
	for (uint32_t binding_id = 0;
		binding_id < ctx->terms->next_binding_id;
		++binding_id) {
		if (!prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, binding_id
			) || prototype_context_contains_binding(
				&ctx->metadata->contexts, match->context_id, binding_id
			)) {
			continue;
		}
		int effect_owned = operation_effect_row_binder_is_owned(
			ctx, equation->payload.classifier.classifier_meta_id, binding_id
		);
		if (effect_owned < 0) {
			return -1;
		}
		if (!effect_owned) {
			return 1;
		}
	}
	*p_classifier_whnf = classifier_whnf;
	return 0;
}

/* Match result equations and effect equations are distinct. A returning
 * computation contributes its result carrier to the motive and its row to the
 * Match join. Other classifiers contribute themselves to the motive. */
static int operation_solver_match_classifier_parts(
	struct compile_context* ctx,
	uint32_t classifier,
	int* p_returns_computation,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
) {
	if (!ctx || !p_returns_computation || !p_result || !p_effect_row ||
		!p_totality ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		*p_returns_computation = 1;
		*p_result = view.result;
		*p_effect_row = view.effect_row;
		*p_totality = view.totality;
		return 0;
	}
	*p_returns_computation = 0;
	*p_result = classifier;
	*p_effect_row = PROTOTYPE_INVALID_ID;
	*p_totality = PROTOTYPE_COMPUTATION_TOTALITY_TOTAL;
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
) {
	int left_returns;
	int right_returns;
	uint32_t left_result;
	uint32_t right_result;
	uint32_t left_row;
	uint32_t right_row;
	int left_totality;
	int right_totality;
	if (!ctx || !p_joined || operation_solver_match_classifier_parts(
			ctx, left, &left_returns, &left_result, &left_row, &left_totality
		) != 0 || operation_solver_match_classifier_parts(
			ctx, right, &right_returns, &right_result, &right_row, &right_totality
		) != 0 || left_returns != right_returns ||
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, left_result, right_result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (!left_returns) {
		*p_joined = left;
		return 0;
	}
	uint32_t joined_row;
	int joined_totality = prototype_computation_totality_join(
		left_totality, right_totality
	);
	if (joined_totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
		return 1;
	}
	return prototype_term_effect_row_union(
			ctx->terms, left_row, right_row, &joined_row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, joined_row, left_result, joined_totality, p_joined
		);
}

/*
 * A uniform motive is represented as a constant lambda. This is not a Match
 * typing shortcut: APP(\_ => T, scrutinee) reduces by beta to T. It is valid
 * only when no branch classifier mentions that branch's pattern binders.
 */
static const struct pending_match_typing* lookup_pending_match_typing(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx) {
		return NULL;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		if (ctx->pending_match_typings[i].operation == operation) {
			return &ctx->pending_match_typings[i];
		}
	}
	return NULL;
}

/*
 * Solve the guarded equation M(C(..., rest, ...)) = M(rest) without first
 * inventing a TermDB classifier for M(rest). The resulting motive carries its
 * own structurally-recursive match frame, so the recursive equation is a
 * finite graph rather than a cyclic solver substitution.
 */
static int operation_solver_validate_guarded_motive_occurrence(
	struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence
) {
	if (!ctx || !ctx->metadata ||
		ih_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		motive_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		argument_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph;
	struct prototype_typed_occurrence_induction_edge edge;
	graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	int edge_status = prototype_typed_occurrence_graph_induction_edge(
			graph, ctx->terms, ih_operation, &edge
		);
	if (edge_status != 0 || edge.owner_match_occurrence != motive_operation ||
		edge.argument_occurrence != argument_occurrence) {
		fprintf(stderr,
			"IH ownership mismatch operation=%u edge-status=%d "
			"edge-owner=%u expected-owner=%u edge-argument=%u expected-argument=%u\n",
			ih_operation, edge_status,
			edge_status == 0 ? edge.owner_match_occurrence : PROTOTYPE_INVALID_ID,
			motive_operation,
			edge_status == 0 ? edge.argument_occurrence : PROTOTYPE_INVALID_ID,
			argument_occurrence);
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[ih_operation];
		uint32_t constraint_id = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < ctx->constraints.classifier_constraint_count; ++i) {
			const struct operation_constraint* constraint =
				&ctx->constraints.constraints[i];
			if (constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
				constraint->payload.classifier.classifier_meta_id == ih_operation) {
				constraint_id = constraint->id;
				break;
			}
		}
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP,
			operation->source_ast,
			ih_operation,
			constraint_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			operation->source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[operation->source_ast].span :
				(struct prototype_source_span) { 0, 0 }
		);
		return -1;
	}
	return 0;
}

static int operation_solver_nonrecursive_seed_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	const struct prototype_typed_occurrence* operation,
	uint32_t* p_classifier,
	uint32_t* p_source_body_occurrence
) {
	if (!ctx || !operation || !p_classifier || !p_source_body_occurrence ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count || operation->case_count == 0 ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	uint32_t candidate = PROTOTYPE_INVALID_ID;
	uint32_t source_body_occurrence = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.classifier_metas[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		int classifier_is_provisional = 0;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			int provisional_status =
				operation_solver_constructor_provisional_classifier(
					ctx, equation->payload.classifier.goal.motive_case.branch_operation,
					&branch_classifier
				);
			if (provisional_status < 0) {
				return -1;
			}
			classifier_is_provisional = provisional_status == 0;
		}
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		int has_recursive_binder = 0;
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				has_recursive_binder = 1;
				break;
			}
		}
		if (has_recursive_binder && !classifier_is_provisional) {
			continue;
		}
		if (candidate == PROTOTYPE_INVALID_ID) {
			candidate = branch_classifier;
			source_body_occurrence = equation->payload.classifier.goal.motive_case.branch_operation;
		} else {
			uint32_t joined;
			int join_status = operation_solver_join_match_classifiers(
				ctx, candidate, branch_classifier, &joined
			);
			if (join_status != 0) {
				return join_status;
			}
			candidate = joined;
		}
	}
	if (candidate == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = candidate;
	*p_source_body_occurrence = source_body_occurrence;
	return 0;
}

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation
) {
	if (!ctx || !operation || operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[operation->core_term];
	for (uint32_t case_index = 0; case_index < match->as.match.case_count; ++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_solver_has_guarded_recursive_equation(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	int has_unresolved_ih = 0;
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation) {
			fprintf(stderr,
				"match solve: missing motive equation match=%u case=%u state=%u constraints=%u\n",
				operation_id, case_index,
				ctx->classifier_solver.motive_solutions[operation_id].state,
				ctx->constraints.classifier_constraint_count);
			return -1;
		}
		if (ctx->classifier_solver.classifier_metas[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier !=
			PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_typed_occurrence* body =
			&ctx->metadata->typed_occurrences.occurrences[
				equation->payload.classifier.goal.motive_case.branch_operation
			];
		if (body->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			body->ih_owner_occurrence != operation_id) {
			return 0;
		}
		has_unresolved_ih = 1;
	}
	return has_unresolved_ih;
}

/*
 * Match solving records only an equation solution state. It deliberately does
 * not build a motive or an IH classifier in TermDB: an unresolved ?M is a
 * solver object, not a graph node. Materialization happens after propagation
 * reaches a fixed point.
 */
static int operation_solver_solve_match(
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
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !typing ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	if (ctx->classifier_solver.classifier_metas[operation_id].classifier !=
			PROTOTYPE_INVALID_ID ||
		ctx->classifier_solver.motive_solutions[operation_id].state ==
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int has_unresolved_body = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		uint32_t body_occurrence = equation->payload.classifier.goal.motive_case.branch_operation;
		if (body_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->classifier_solver.classifier_metas[body_occurrence].classifier ==
				PROTOTYPE_INVALID_ID) {
			has_unresolved_body = 1;
		}
	}
	if (!has_unresolved_body) {
		ctx->classifier_solver.motive_solutions[operation_id].state =
			OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY;
		return 0;
	}
	int has_recursive_binder = operation_solver_match_has_recursive_binder(ctx, operation);
	if (has_recursive_binder < 0) {
		fprintf(stderr, "match solve: recursive binder inspection failed match=%u\n",
			operation_id);
		return -1;
	}
	if (!has_recursive_binder) {
		return 0;
	}
	/* A guarded recursive-use equation is stronger than the one-branch bootstrap:
	 * it determines the indexed abstraction itself. Do not replace that READY
	 * solution with a PROVISIONAL constant candidate while the fixed point makes
	 * another pass over the same Match. */
	if (ctx->classifier_solver.motive_solutions[operation_id].state ==
			OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY &&
		ctx->classifier_solver.motive_solutions[operation_id].motive < ctx->terms->term_count) {
		return 0;
	}
	/*
	 * A non-recursive branch can constrain a constant motive before the
	 * recursive branch is classified.  This is the normal constraint-solving
	 * path for CBPV lowering: the recursive occurrence may be below RETURN,
	 * THUNK, or COMPUTATION_FOLD, so it must receive M(rest) through the solver before its
	 * enclosing computation can be classified.  Do this before considering the
	 * old direct-IH fallback, which only understands an IH as the immediate
	 * branch operation.
	 */
	uint32_t seed_classifier;
	uint32_t seed_source_body_occurrence;
	int seed_status = operation_solver_nonrecursive_seed_classifier(
		ctx, operation_id, operation, &seed_classifier, &seed_source_body_occurrence
	);
	if (seed_status < 0) {
		fprintf(stderr, "match solve: nonrecursive seed failed match=%u\n",
			operation_id);
		return -1;
	}
	if (seed_status == 0) {
		seed_status = operation_solver_seed_motive(
			ctx, operation_id, seed_classifier, seed_source_body_occurrence, p_changed
		);
		if (seed_status < 0) {
			fprintf(stderr,
				"match solve: motive seed failed match=%u classifier=%u source=%u\n",
				operation_id, seed_classifier, seed_source_body_occurrence);
		}
		if (seed_status < 0) {
			return -1;
		}
		if (seed_status == 0) {
			return 0;
		}
		ctx->classifier_solver.motive_solutions[operation_id].state =
			OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE;
		return 0;
	}
	int guarded_status = operation_solver_has_guarded_recursive_equation(ctx, operation_id);
	if (guarded_status < 0) {
		fprintf(stderr, "match solve: guarded equation inspection failed match=%u\n",
			operation_id);
		return -1;
	}
	if (guarded_status > 0) {
		ctx->classifier_solver.motive_solutions[operation_id].state =
			OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE;
		return 0;
	}
	return 0;
}
