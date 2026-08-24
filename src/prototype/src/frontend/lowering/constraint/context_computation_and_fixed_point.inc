static int operation_solver_context_classifier_from_lift(
	struct compile_context* ctx,
	uint32_t context_id,
	const uint32_t* relocation,
	uint32_t resolved_context_count,
	const uint32_t* first_lift_by_source,
	const uint32_t* next_lift,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !relocation || !first_lift_by_source ||
		!next_lift || !p_classifier ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	const struct prototype_context* context = prototype_context_get(
		&ctx->metadata->contexts, context_id
	);
	if (!context) {
		return -1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t substitution_id = first_lift_by_source[context_id];
		substitution_id != PROTOTYPE_INVALID_ID;
		substitution_id = next_lift[substitution_id]) {
		if (substitution_id >=
			ctx->metadata->substitutions.substitution_count) {
			return -1;
		}
		const struct prototype_substitution* substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution_id
			);
		if (!substitution ||
			substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->source_context != context_id ||
			substitution->target_context == context_id ||
			substitution->term >= ctx->terms->term_count ||
			ctx->terms->terms[substitution->term].tag != PROTOTYPE_TERM_VAR ||
			ctx->terms->terms[substitution->term].as.var.binding_id !=
				context->binding_id) {
			continue;
		}
		const struct prototype_context* target = prototype_context_get(
			&ctx->metadata->contexts, substitution->target_context
		);
		const struct prototype_substitution* prefix =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution->first
			);
		if (!target || !prefix || target->binding_id != context->binding_id ||
			target->parent != prefix->target_context ||
			prefix->source_context != context_id) {
			continue;
		}
		uint32_t target_classifier = prototype_context_classifier_term(target);
		if (target_classifier == PROTOTYPE_INVALID_ID &&
			substitution->target_context < resolved_context_count) {
			const struct prototype_context* resolved_target = prototype_context_get(
				&ctx->metadata->contexts,
				relocation[substitution->target_context]
			);
			target_classifier = resolved_target ?
				prototype_context_classifier_term(resolved_target) :
				PROTOTYPE_INVALID_ID;
		}
		if (target_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t candidate;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				target_classifier,
				substitution->first,
				&candidate
			) != 0) {
			return -1;
		}
		if (classifier != PROTOTYPE_INVALID_ID &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				classifier,
				candidate
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		classifier = candidate;
	}
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = classifier;
	return 0;
}

static int operation_solver_resolve_contexts(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->contexts.context_count > PROTOTYPE_CONTEXT_CAPACITY ||
		ctx->metadata->substitutions.substitution_count >
			PROTOTYPE_SUBSTITUTION_CAPACITY) {
		return -1;
	}
	ctx->metadata->context_resolution_request_count++;
	if (ctx->context_resolution_valid &&
		ctx->context_resolution_dependency_revision ==
			ctx->context_dependency_revision &&
		ctx->context_resolution_context_count ==
			ctx->metadata->contexts.context_count &&
		(!finalize || ctx->context_resolution_finalized)) {
		ctx->metadata->context_resolution_skip_count++;
		return 0;
	}
	ctx->metadata->context_resolution_pass_count++;
	if (operation_binder_owners_ensure(ctx) != 0) {
		return -1;
	}
	/* Existing substitutions are immutable.  Rebased morphisms are interned as
	 * new records, so reindex results keyed by old IDs remain valid. */
	uint32_t relocation[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t context_dirty[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t context_semantics_changed[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t substitution_live[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint8_t substitution_rebased[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t substitution_relocation[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t first_lift_by_source[PROTOTYPE_CONTEXT_CAPACITY];
	uint32_t next_lift[PROTOTYPE_SUBSTITUTION_CAPACITY];
	size_t source_count = ctx->metadata->contexts.context_count;
	size_t substitution_count =
		ctx->metadata->substitutions.substitution_count;
	int all_contexts_dirty = ctx->context_dependencies_all_dirty ||
		ctx->context_resolution_context_count == 0 ||
		(finalize && !ctx->context_resolution_finalized);
	int trace_context_resolution =
		getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE") != NULL;
	if (trace_context_resolution) {
		fprintf(
			stderr,
			"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 revision=%" PRIu64
			" source_contexts=%zu all_dirty=%d finalize=%d\n",
			ctx->context_dependency_revision, source_count,
			all_contexts_dirty, finalize
		);
	}
	memset(substitution_live, 0, sizeof(substitution_live));
#define MARK_LIVE_SUBSTITUTION(substitution_id) \
	do { \
		uint32_t live_id = (substitution_id); \
		if (live_id != PROTOTYPE_INVALID_ID) { \
			if (live_id >= substitution_count) { \
				return -1; \
			} \
			substitution_live[live_id] = 1; \
		} \
	} while (0)
	/* SubstitutionDB is a monotone candidate arena. Context resolution must
	 * update only actions reachable from the current constraint/proof graph;
	 * rejected alternatives are not semantic roots and may contain deliberately
	 * incomplete candidate payloads. */
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		MARK_LIVE_SUBSTITUTION(
			ctx->metadata->typed_occurrences.occurrences[i].
				context_action_substitution
		);
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.case_count;
		++i) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[i];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			MARK_LIVE_SUBSTITUTION(operation_case->refinement_substitution);
		}
	}
	for (uint32_t i = 0; i < ctx->constraints.constraint_count; ++i) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT &&
			constraint->payload.branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
			MARK_LIVE_SUBSTITUTION(
				constraint->payload.branch_refinement.substitution
			);
		}
	}
	/* Accepted Derivations retain the immutable Context morphisms under which
	 * they were checked. Only mutable candidate roots participate in rebasing. */
	for (size_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		const struct prototype_judgement_derivation_candidate* derivation =
			&ctx->judgement_delta.derivation_candidates[i];
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			MARK_LIVE_SUBSTITUTION(derivation->semantic_action_id);
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				MARK_LIVE_SUBSTITUTION(
					derivation->premises[j].semantic_action_id
				);
			}
		}
	}
	for (size_t reverse = substitution_count; reverse > 0; --reverse) {
		uint32_t substitution_id = (uint32_t)(reverse - 1);
		if (!substitution_live[substitution_id]) {
			continue;
		}
		const struct prototype_substitution* substitution =
			&ctx->metadata->substitutions.substitutions[substitution_id];
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			MARK_LIVE_SUBSTITUTION(substitution->first);
		}
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			MARK_LIVE_SUBSTITUTION(substitution->second);
		}
	}
#undef MARK_LIVE_SUBSTITUTION
	for (uint32_t context_id = 0; context_id < source_count; ++context_id) {
		first_lift_by_source[context_id] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t substitution_id = 0;
		substitution_id < ctx->metadata->substitutions.substitution_count;
		++substitution_id) {
		next_lift[substitution_id] = PROTOTYPE_INVALID_ID;
		const struct prototype_substitution* substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution_id
			);
		if (!substitution ||
			!substitution_live[substitution_id] ||
			substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->source_context >= source_count) {
			continue;
		}
		next_lift[substitution_id] =
			first_lift_by_source[substitution->source_context];
		first_lift_by_source[substitution->source_context] = substitution_id;
	}
	relocation[0] = 0;
	context_dirty[0] = all_contexts_dirty;
	context_semantics_changed[0] = 0;
	for (uint32_t context_id = 1; context_id < source_count; ++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context || context->parent >= context_id) {
			fprintf(stderr, "context resolution invalid context=%u parent=%u source-count=%zu\n",
				context_id, context ? context->parent : PROTOTYPE_INVALID_ID,
				source_count);
			return -1;
		}
		int binding_dirty = operation_context_binding_is_dirty(
			ctx, context->binding_id
		);
		context_dirty[context_id] = all_contexts_dirty ||
			context_id >= ctx->context_resolution_context_count ||
			context_dirty[context->parent] ||
			binding_dirty;
		if (!context_dirty[context_id]) {
			relocation[context_id] = context_id;
			context_semantics_changed[context_id] = 0;
			continue;
		}
		ctx->metadata->context_resolution_context_visit_count++;
		if (trace_context_resolution) {
			fprintf(
				stderr,
				"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 revision=%" PRIu64
				" context=%u parent=%u binding=%u cause=%s\n",
				ctx->context_dependency_revision, context_id, context->parent,
				context->binding_id,
				all_contexts_dirty ? "global" :
					(context_id >= ctx->context_resolution_context_count ? "new" :
						(binding_dirty ? "binding" : "parent"))
			);
		}
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		/* A concrete Context classifier is already expressed in its parent
		 * Context.  In particular, comprehension has reindexed dependent binder
		 * types through the branch action.  BindingId identifies the binder
		 * object, but it does not make those context-indexed classifier terms
		 * interchangeable.  Consult the owner only for an unresolved extension. */
		int classifier_variable_resolved = 0;
		uint32_t lifted_classifier;
		int lift_status = operation_solver_context_classifier_from_lift(
			ctx,
			context_id,
			relocation,
			context_id,
			first_lift_by_source,
			next_lift,
			&lifted_classifier
		);
		if (lift_status < 0) {
			return -1;
		}
		if (lift_status == 0) {
			classifier = lifted_classifier;
			classifier_variable_resolved = 1;
		} else {
			/* The direct Lambda body Context is a projection of the current solved
			 * binder occurrence. It is first created from the surface annotation, but
			 * classifier/effect constraints may refine that occurrence in a later
			 * fixed-point round. Reindexed branch Contexts may carry a genuinely
			 * different dependent classifier and are deliberately excluded by the
			 * parent check. */
			struct operation_binder_owner_entry* owner =
				operation_binder_owner_find(ctx, context->binding_id);
			if (owner && owner->lambda_occurrence != PROTOTYPE_INVALID_ID) {
				if (owner->lambda_occurrence >=
						ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				const struct prototype_typed_occurrence* lambda =
					&ctx->metadata->typed_occurrences.occurrences[
						owner->lambda_occurrence
					];
				if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
					lambda->binding_id != context->binding_id) {
					return -1;
				}
				if (context->parent == lambda->context_id &&
					operation_solver_binder_classifier(
						ctx, owner->lambda_occurrence
					) != PROTOTYPE_INVALID_ID) {
					classifier = operation_solver_binder_classifier(
						ctx, owner->lambda_occurrence
					);
					classifier_variable_resolved = 1;
				}
			}
			if (!classifier_variable_resolved &&
				classifier != PROTOTYPE_INVALID_ID) {
				classifier_variable_resolved = 1;
			}
		}
		if (!classifier_variable_resolved) {
			uint32_t owner_classifier;
			int owner_status = operation_binder_owner_classifier(
				ctx, context->binding_id, &owner_classifier
			);
			if (owner_status < 0) {
				return -1;
			}
			if (owner_status == 0) {
				classifier = owner_classifier;
				classifier_variable_resolved = 1;
			}
		}
		if (classifier_variable != PROTOTYPE_INVALID_ID) {
			int occurrence_classifier_resolved = 0;
			struct operation_binder_owner_entry* evidence_owner =
				operation_binder_owner_find(ctx, context->binding_id);
			for (uint32_t evidence_id = evidence_owner ?
					evidence_owner->first_classifier_evidence :
					PROTOTYPE_INVALID_ID;
				evidence_id != PROTOTYPE_INVALID_ID;
				evidence_id =
					ctx->binder_classifier_evidence[evidence_id].next) {
				if (evidence_id >= ctx->binder_classifier_evidence_count) {
					return -1;
				}
				uint32_t operation_id =
					ctx->binder_classifier_evidence[evidence_id].occurrence_id;
				if (operation_id >=
					ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				const struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				uint32_t operation_classifier =
					operation_available_classifier(ctx, operation_id);
				uint32_t operation_binding_context;
				if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
					operation->referenced_ast_binder_id != classifier_variable ||
					operation->binding_id != context->binding_id ||
					operation_classifier == PROTOTYPE_INVALID_ID ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						operation->context_id,
						operation->binding_id,
						&operation_binding_context
					) != 0 || operation_binding_context != context_id) {
					continue;
				}
				if (occurrence_classifier_resolved &&
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						operation_classifier
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					fprintf(stderr,
						"context resolution occurrence classifier conflict "
						"context=%u operation=%u\n",
						context_id, operation_id);
					return -1;
				}
				classifier = operation_classifier;
				occurrence_classifier_resolved = 1;
				classifier_variable_resolved = 1;
			}
			for (uint32_t pending_id = 0;
				!classifier_variable_resolved &&
				pending_id < ctx->pending_binder_assumption_count;
				++pending_id) {
				const struct pending_binder_assumption* pending =
					&ctx->pending_binder_assumptions[pending_id];
				uint32_t assumption_context_id;
				if (pending->binder_var >= ctx->terms->term_count ||
					ctx->terms->terms[pending->binder_var].tag !=
						PROTOTYPE_TERM_VAR ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						pending->context_id,
						ctx->terms->terms[pending->binder_var].as.var.binding_id,
						&assumption_context_id
					) != 0 || assumption_context_id != context_id) {
					continue;
				}
				if (classifier_variable_resolved &&
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						pending->classifier
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					fprintf(stderr,
						"context resolution assumption classifier conflict context=%u "
						"pending=%u binding=%u classifier=%u pending-classifier=%u "
						"source-operation=%u\n",
						context_id, pending_id, context->binding_id, classifier,
						pending->classifier, pending->source_operation);
					return -1;
				}
				classifier = pending->classifier;
				classifier_variable_resolved = 1;
			}
			/* A provisional Context may already carry the source-scope classifier.
			 * Refined occurrences use distinct Context extensions and must not
			 * overwrite that source authority merely because they retain the same
			 * surface binder identity. */
			if (!classifier_variable_resolved &&
				classifier != PROTOTYPE_INVALID_ID) {
				classifier_variable_resolved = 1;
			}
			for (int source = 0;
				classifier == PROTOTYPE_INVALID_ID && source < 2;
				++source) {
				const struct prototype_judgement_proposition* relations = source == 0 ?
					ctx->judgement_delta.propositions : ctx->judgement->propositions;
				const struct prototype_judgement_derivation_candidate* derivations = source == 0 ?
					ctx->judgement_delta.derivation_candidates :
					ctx->judgement->derivation_candidates;
				size_t proposition_count = source == 0 ?
					ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
				size_t derivation_candidate_count = source == 0 ?
					ctx->judgement_delta.derivation_candidate_count :
					ctx->judgement->derivation_candidate_count;
				for (size_t relation_id = 0; relation_id < proposition_count; ++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&relations[relation_id];
					if (relation->context_id != context_id ||
						relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						prototype_judgement_candidate_find_derivation_kind(
							relations,
							proposition_count,
							derivations,
							derivation_candidate_count,
							(uint32_t)relation_id,
							PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
							&(uint32_t){0}
						) != 0 ||
						relation->subject >= ctx->terms->term_count ||
						ctx->terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
						ctx->terms->terms[relation->subject].as.var.binding_id !=
							context->binding_id) {
						continue;
					}
					classifier = relation->classifier;
					classifier_variable_resolved = 1;
					break;
				}
			}
		}
		size_t context_count_before = ctx->metadata->contexts.context_count;
		uint32_t producer_computation = context->producer_computation;
		if (context->extension_kind ==
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) {
			struct operation_binder_owner_entry* result_owner =
				operation_binder_owner_find(ctx, context->binding_id);
			if (result_owner && result_owner->bound_computation_occurrence !=
					PROTOTYPE_INVALID_ID &&
				result_owner->bound_computation_occurrence >=
					ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			if (result_owner && result_owner->bound_computation_occurrence !=
					PROTOTYPE_INVALID_ID) {
				producer_computation = ctx->metadata->typed_occurrences.occurrences[
					result_owner->bound_computation_occurrence
				].core_term;
			}
		}
		int context_extend_status = context->extension_kind ==
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
			prototype_context_extend_sequence_result(
				&ctx->metadata->contexts,
				relocation[context->parent],
				context->binding_id,
				classifier,
				finalize && classifier_variable_resolved ?
					PROTOTYPE_INVALID_ID : classifier_variable,
				producer_computation,
				&relocation[context_id]
			) : prototype_context_extend(
				&ctx->metadata->contexts,
				relocation[context->parent],
				context->binding_id,
				classifier,
				finalize && classifier_variable_resolved ?
					PROTOTYPE_INVALID_ID : classifier_variable,
				&relocation[context_id]
			);
		if (context_extend_status != 0) {
			fprintf(stderr, "context resolution extension failed context=%u parent=%u classifier=%u variable=%u\n",
				context_id, context->parent, classifier, classifier_variable);
			return -1;
		}
		if (ctx->metadata->contexts.context_count != context_count_before) {
			ctx->metadata->context_resolution_context_insert_count++;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->context_resolution_context_change_count++;
		}
		context_semantics_changed[context_id] =
			context_semantics_changed[context->parent] ||
			(prototype_context_classifier_term(context) != PROTOTYPE_INVALID_ID &&
			 prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				prototype_context_classifier_term(context),
				classifier
			 ).status != PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.occurrences[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.occurrences[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->constraints.constraint_count; ++i) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		uint32_t context_id = constraint->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			constraint->context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT) {
			uint32_t source_context =
				constraint->payload.branch_refinement.source_context;
			uint32_t refined_context =
				constraint->payload.branch_refinement.refined_context;
			if (source_context >= source_count ||
				(refined_context != PROTOTYPE_INVALID_ID &&
				 refined_context >= source_count)) {
				return -1;
			}
			if (relocation[source_context] != source_context) {
				constraint->payload.branch_refinement.source_context =
					relocation[source_context];
				ctx->metadata->context_resolution_root_projection_count++;
			}
			if (refined_context != PROTOTYPE_INVALID_ID) {
				if (relocation[refined_context] != refined_context) {
					constraint->payload.branch_refinement.refined_context =
						relocation[refined_context];
					ctx->metadata->context_resolution_root_projection_count++;
				}
			}
		}
	}
	/* Context canonicalization may turn a previously unresolved comprehension
	 * classifier into a concrete binder type. The occurrence topology and all
	 * unrelated classifier solutions remain valid; refresh only the persistent
	 * binder-lookup constraints and wake their reverse dependents. */
	for (uint32_t id = 0;
		id < ctx->constraints.classifier_constraint_count;
		++id) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			constraint->kind != OPERATION_CONSTRAINT_BINDER_TYPE) {
			continue;
		}
		uint32_t operation_id = constraint->source_occurrence;
		uint32_t binding_context;
		int binding_status = operation_solver_var_binding_context(
			ctx, operation_id, &binding_context
		);
		if (binding_status < 0) {
			fprintf(stderr,
				"context binder projection lookup failed operation=%u context=%u\n",
				operation_id,
				ctx->metadata->typed_occurrences.occurrences[operation_id].context_id);
			return -1;
		}
		constraint->payload.classifier.goal.binder_type.binding_context =
			binding_status == 0 ? binding_context : PROTOTYPE_INVALID_ID;
	}
	for (uint32_t operation_id = 0;
		ctx->classifier_solver.initialized &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
			ctx->classifier_solver.classifier_metas[operation_id].classifier !=
				PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t binding_context;
		int binding_status = operation_solver_var_binding_context(
			ctx, operation_id, &binding_context
		);
		if (binding_status < 0) {
			return -1;
		}
		if (binding_status != 0) {
			continue;
		}
		const struct prototype_context* binding = prototype_context_get(
			&ctx->metadata->contexts, binding_context
		);
		if (!binding) {
			fprintf(stderr,
				"context binder projection missing Context operation=%u binding-context=%u\n",
				operation_id, binding_context);
			return -1;
		}
		uint32_t classifier = prototype_context_classifier_term(binding);
		if (classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (operation_classifier_meta_solve(
				ctx, operation_id, classifier
			) != 0) {
			return -1;
		}
		if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
			fprintf(stderr,
				"context binder projection enqueue failed operation=%u\n",
				operation_id);
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.case_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.cases[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.cases[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.fold_clause_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.fold_clauses[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.fold_clauses[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		uint32_t context_id = ctx->pending_binder_assumptions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->pending_binder_assumptions[i].context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		uint32_t context_id = ctx->pending_declaration_facts[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->pending_declaration_facts[i].context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement_delta.propositions[i];
		uint32_t context_id = proposition->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			proposition->context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	/* Rebase live morphisms into new immutable records. The original payload and
	 * ID remain valid for already accepted proof edges. Components are ordered
	 * before users by the SubstitutionDB invariant, so one forward pass suffices. */
	for (uint32_t i = 0; i < substitution_count; ++i) {
		substitution_relocation[i] = i;
		substitution_rebased[i] = 0;
		const struct prototype_substitution* substitution =
			prototype_substitution_get(&ctx->metadata->substitutions, i);
		if (!substitution || !substitution_live[i]) {
			continue;
		}
		if (substitution->source_context >= source_count ||
			substitution->target_context >= source_count ||
			((substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			  substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) &&
			 substitution->first >= i) ||
			(substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE &&
			 substitution->second >= i)) {
			return -1;
		}
		uint32_t source_context = relocation[substitution->source_context];
		uint32_t target_context = relocation[substitution->target_context];
		uint32_t first = substitution->first == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INVALID_ID : substitution_relocation[substitution->first];
		uint32_t second = substitution->second == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INVALID_ID : substitution_relocation[substitution->second];
		if (context_semantics_changed[substitution->source_context] ||
			context_semantics_changed[substitution->target_context] ||
			((substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			  substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) &&
			 !substitution_rebased[substitution->first]) ||
			(substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE &&
			 !substitution_rebased[substitution->second])) {
			continue;
		}
		uint32_t term_classifier = substitution->term_classifier;
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
			substitution->term < ctx->terms->term_count &&
			ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_VAR) {
			uint32_t binding_context_id;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					source_context,
					ctx->terms->terms[substitution->term].as.var.binding_id,
					&binding_context_id
				) != 0) {
				fprintf(stderr,
					"context resolution found out-of-scope substitution term "
					"substitution=%u source=%u target=%u first=%u term=%u "
					"binding=%u\n",
					i, source_context, target_context,
					substitution->first, substitution->term,
					ctx->terms->terms[substitution->term].as.var.binding_id);
				return -1;
			}
			const struct prototype_context* binding_context = prototype_context_get(
				&ctx->metadata->contexts, binding_context_id
			);
			if (!binding_context) {
				return -1;
			}
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding_context);
			if (binding_classifier == PROTOTYPE_INVALID_ID) {
				/* Context resolution participates in the classifier fixed point.  A
				 * branch substitution may therefore mention a binder whose classifier
				 * is still pending in a non-final pass.  Preserve that pending payload;
				 * the final pass below remains the publication boundary that rejects it. */
				if (finalize) {
					return -1;
				}
					term_classifier = PROTOTYPE_INVALID_ID;
				} else {
				term_classifier = binding_classifier;
				}
			}
		if (source_context == substitution->source_context &&
			target_context == substitution->target_context &&
			first == substitution->first && second == substitution->second &&
			term_classifier == substitution->term_classifier) {
			substitution_rebased[i] = 1;
			continue;
		}
		ctx->metadata->context_resolution_substitution_visit_count++;
		size_t substitution_count_before =
			ctx->metadata->substitutions.substitution_count;
		if (prototype_substitution_rebase(
				&ctx->metadata->substitutions,
				i,
				source_context,
				target_context,
				first,
				second,
				term_classifier,
				&substitution_relocation[i]
			) != 0) {
			return -1;
		}
		substitution_rebased[i] = 1;
		if (ctx->metadata->substitutions.substitution_count !=
			substitution_count_before) {
			ctx->metadata->context_resolution_substitution_insert_count++;
		}
		if (substitution_relocation[i] != i) {
			ctx->metadata->context_resolution_substitution_rebase_count++;
		}
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
			if (operation->context_action_substitution >= substitution_count ||
				!substitution_live[operation->context_action_substitution]) {
				return -1;
			}
			uint32_t action = operation->context_action_substitution;
			operation->context_action_substitution = substitution_relocation[action];
			if (operation->context_action_substitution != action) {
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.case_count;
		++i) {
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[i];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			if (operation_case->refinement_substitution >= substitution_count ||
				!substitution_live[operation_case->refinement_substitution]) {
				return -1;
			}
			uint32_t action = operation_case->refinement_substitution;
			operation_case->refinement_substitution = substitution_relocation[action];
			if (operation_case->refinement_substitution != action) {
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
	}
	int candidate_actions_changed = 0;
	for (size_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		struct prototype_judgement_derivation_candidate* derivation =
			&ctx->judgement_delta.derivation_candidates[i];
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			if (derivation->semantic_action_id >= substitution_count ||
				!substitution_live[derivation->semantic_action_id]) {
				return -1;
			}
			uint32_t action = derivation->semantic_action_id;
			derivation->semantic_action_id = substitution_relocation[action];
			if (derivation->semantic_action_id != action) {
				candidate_actions_changed = 1;
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				uint32_t action = derivation->premises[j].semantic_action_id;
				if (action >= substitution_count || !substitution_live[action]) {
					return -1;
				}
				derivation->premises[j].semantic_action_id =
					substitution_relocation[action];
				if (derivation->premises[j].semantic_action_id != action) {
					candidate_actions_changed = 1;
					ctx->metadata->context_resolution_root_projection_count++;
				}
			}
		}
	}
	if (candidate_actions_changed &&
		prototype_judgement_delta_rebuild_candidate_index(
			&ctx->judgement_delta
		) != 0) {
		return -1;
	}
	/* Mutable branch constraints follow the rebased action. Accepted proof edges
	 * continue to denote their original immutable morphisms. */
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->payload.branch_refinement.refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
			continue;
		}
		uint32_t old_action =
			constraint->payload.branch_refinement.substitution;
		if (old_action >= substitution_count || !substitution_live[old_action]) {
			return -1;
		}
		constraint->payload.branch_refinement.substitution =
			substitution_relocation[old_action];
		if (constraint->payload.branch_refinement.substitution != old_action) {
			ctx->metadata->context_resolution_root_projection_count++;
		}
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			constraint->payload.branch_refinement.substitution
		);
		if (!action) {
			return -1;
		}
		constraint->payload.branch_refinement.source_context =
			action->target_context;
		constraint->payload.branch_refinement.refined_context =
			action->source_context;
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->context_action_substitution == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			operation->context_action_substitution
		);
		if (!action) {
			return -1;
		}
		if (action->source_context != operation->context_id) {
			/* A changed binder classifier invalidates this mutable projection. The
			 * original immutable action remains available to accepted evidence, while
			 * ordinary constraints rebuild the occurrence in its final Context. */
			operation->context_action_substitution = PROTOTYPE_INVALID_ID;
			operation->source_core_term = PROTOTYPE_INVALID_ID;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
		}
	}
	for (size_t i = 0; i < ctx->judgement_delta.derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof =
			&ctx->judgement_delta.derivation_candidates[i];
		if (proof->conclusion_proposition_id >=
			ctx->judgement_delta.proposition_count) {
			return -1;
		}
		proof->conclusion = &ctx->judgement_delta.propositions[
			proof->conclusion_proposition_id
		];
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			((proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ?
				proof->premise_count != 0 : proof->premise_count != 1) ||
			 proof->semantic_action_id >=
				ctx->metadata->substitutions.substitution_count)) {
			return -1;
		}
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		uint32_t context_id =
			ctx->judgement_delta.computation_constraints[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->judgement_delta.computation_constraints[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	ctx->context_resolution_dependency_revision =
		ctx->context_dependency_revision;
	ctx->context_resolution_context_count =
		ctx->metadata->contexts.context_count;
	ctx->context_dirty_binding_count = 0;
	ctx->context_dependencies_all_dirty = 0;
	ctx->context_resolution_valid = 1;
	ctx->context_resolution_finalized = finalize;
	return 0;
}

static int operation_solver_generate_computation_constraints_impl(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* Branch actions may reindex an operation after an earlier computation
	 * payload was recorded. JudgementDelta keeps stable record slots, but only
	 * the payload matching the current typed occurrence is an active mutable
	 * constraint. Retire stale slots before rebuilding ConstraintDB links. */
	for (size_t constraint_id = 0;
		constraint_id < ctx->judgement_delta.computation_constraint_count;
		++constraint_id) {
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[constraint_id];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[
				constraint->occurrence_id
			];
		if (operation->core_term != constraint->subject ||
			operation->context_id != constraint->context_id ||
			(operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST)) {
			constraint->occurrence_id = PROTOTYPE_INVALID_ID;
			constraint->premise_occurrence_count = 0;
			constraint->return_body_occurrence_id = PROTOTYPE_INVALID_ID;
		}
	}
	if (prototype_judgement_delta_rebuild_computation_constraint_index(
			&ctx->judgement_delta
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			continue;
		}
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, i
		);
		int status = prototype_judgement_delta_record_computation_constraint(
			&ctx->judgement_delta,
			ctx->terms,
			operation->context_id,
			operation->core_term
		);
		if (status != 0) {
			return -1;
		}
		uint32_t delta_constraint_id;
		if (prototype_judgement_delta_computation_constraint_for_occurrence(
				&ctx->judgement_delta, i, &delta_constraint_id
			) != 0) {
			return -1;
		}
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[delta_constraint_id];
		uint32_t lifecycle_id =
			ctx->constraints.computation_constraint_for_occurrence[i];
		if (lifecycle_id == PROTOTYPE_INVALID_ID) {
			if (ctx->constraints.constraint_count >=
				PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY) {
				return -1;
			}
			uint32_t id = ctx->constraints.constraint_count++;
			ctx->constraints.computation_constraint_count++;
			lifecycle_id = id;
			ctx->constraints.constraints[id] = (struct operation_constraint){
				.id = id,
				.domain = OPERATION_CONSTRAINT_DOMAIN_COMPUTATION,
				.kind = constraint->kind,
				.source_occurrence = i,
				.source_ast = operation->source_ast,
				.context_id = operation->context_id,
				.state = OPERATION_CONSTRAINT_STATE_PENDING,
				.reason = OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY,
				.evidence_id = PROTOTYPE_INVALID_ID,
				.origin_constraint_id = PROTOTYPE_INVALID_ID,
				.payload.computation = {
					.judgement_delta_constraint_id = delta_constraint_id
				}
			};
			ctx->constraints.computation_constraint_for_occurrence[i] = id;
		} else {
			if (lifecycle_id >= ctx->constraints.constraint_count ||
				ctx->constraints.constraints[lifecycle_id].domain !=
					OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
				ctx->constraints.constraints[lifecycle_id].source_occurrence != i) {
				return -1;
			}
			struct operation_constraint* lifecycle =
				&ctx->constraints.constraints[lifecycle_id];
			lifecycle->kind = constraint->kind;
			lifecycle->source_ast = operation->source_ast;
			lifecycle->context_id = operation->context_id;
			lifecycle->payload.computation.judgement_delta_constraint_id =
				delta_constraint_id;
			if (operation_constraint_reopen(
					lifecycle,
					OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
				) != 0) {
				return -1;
			}
		}
		if (lifecycle_id == PROTOTYPE_INVALID_ID ||
			operation_solver_add_constraint_dependency(
				ctx, i, lifecycle_id
			) != 0) {
			return -1;
		}
		constraint->premise_occurrence_count = 0;
		constraint->return_body_occurrence_id = PROTOTYPE_INVALID_ID;
		for (uint32_t premise = 0;
			premise < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES;
			++premise) {
			constraint->premise_contexts[premise] = PROTOTYPE_INVALID_ID;
			constraint->premise_states[premise] =
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
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
			for (uint32_t premise = 0;
				premise < constraint->premise_occurrence_count;
				++premise) {
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->premise_occurrences[premise],
						lifecycle_id
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
		for (uint32_t premise = 0; premise < premise_count; ++premise) {
			if (operation_solver_add_constraint_dependency(
					ctx,
					constraint->premise_occurrences[premise],
					lifecycle_id
				) != 0) {
				return -1;
			}
		}
	}
	ctx->constraints.first_effect_constraint = ctx->constraints.constraint_count;
	return operation_constraint_db_validate(ctx);
}

static int operation_solver_generate_computation_constraints(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	ctx->metadata->computation_constraint_generation_pass_count++;
	/* Effect equations are regenerated after computation solving. Remove the
	 * previous effect suffix before adding any newly discovered computation
	 * obligations; classifier and existing computation identities remain
	 * stable across fixed-point rounds. */
	ctx->constraints.constraint_count = ctx->constraints.first_effect_constraint;
	ctx->constraints.effect_constraint_count = 0;
	struct operation_constraint_db_mark mark =
		operation_constraint_db_mark(&ctx->constraints);
	int status = operation_solver_generate_computation_constraints_impl(ctx);
	if (status != 0 && operation_constraint_db_rollback(
			&ctx->constraints, mark
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
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t lifecycle_id = constraint->occurrence_id < 4096 ?
			ctx->constraints.computation_constraint_for_occurrence[
				constraint->occurrence_id
			] : PROTOTYPE_INVALID_ID;
		if (lifecycle_id == PROTOTYPE_INVALID_ID ||
			lifecycle_id >= ctx->constraints.constraint_count) {
			fprintf(stderr,
				"computation binding lifecycle missing delta=%zu total-delta=%zu first=%u count=%u total=%u\n",
				i, ctx->judgement_delta.computation_constraint_count,
				ctx->constraints.first_computation_constraint,
				ctx->constraints.computation_constraint_count,
				ctx->constraints.constraint_count);
			return -1;
		}
		struct operation_constraint* lifecycle =
			&ctx->constraints.constraints[lifecycle_id];
		if (lifecycle->domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			lifecycle->payload.computation.judgement_delta_constraint_id != i) {
			return -1;
		}
		int state = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CONSTRAINT_STATE_PENDING :
			OPERATION_CONSTRAINT_STATE_SOLVED;
		int reason = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
			OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
		if (operation_constraint_transition(lifecycle, state, reason) != 0) {
			fprintf(stderr,
				"computation binding lifecycle transition delta=%zu current=%d target=%d\n",
				i, lifecycle->state, state);
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
			uint32_t current = operation_solver_classifier(ctx, operation_id);
			uint32_t snapshot = constraint->premise_classifiers[premise];
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
		if (constraint->occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count ||
			result->solved_classifier >= ctx->terms->term_count ||
			operation_solver_bind_proven_classifier(
				ctx,
				constraint->occurrence_id,
				result->solved_classifier,
				p_changed
			) != 0) {
			fprintf(stderr,
				"computation binding publication failed delta=%zu occurrence=%u result=%u occurrence-count=%zu term-count=%zu\n",
				i, constraint->occurrence_id, result->solved_classifier,
				ctx->metadata->typed_occurrences.occurrence_count,
				ctx->terms->term_count);
			return -1;
		}
	}
	}
	return 0;
}

static int operation_solver_refresh_computation_constraint_operands(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			if (constraint->occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			if (i >= ctx->computation_constraint_result_capacity) {
				return -1;
			}
			ctx->computation_constraint_results[i].projected_classifier =
				operation_solver_classifier(
				ctx, constraint->occurrence_id
			);
		}
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
			uint32_t classifier = operation_solver_classifier(ctx, operation_id);
			constraint->premise_classifiers[premise] = classifier;
			memset(
				&constraint->premise_evidence[premise],
				0,
				sizeof(constraint->premise_evidence[premise])
			);
			constraint->premise_contexts[premise] =
				ctx->metadata->typed_occurrences.occurrences[operation_id].context_id;
			if (classifier == PROTOTYPE_INVALID_ID) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
				continue;
			}
			int local = operation_classifier_contains_unowned_effect_row(
				ctx, operation_id, classifier
			);
			if (local < 0) {
				return -1;
			}
			if (local) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL;
				continue;
			}
			int evidence_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				operation_id,
				constraint->context_id,
				ctx->metadata->typed_occurrences.occurrences[operation_id].core_term,
				classifier,
				&constraint->premise_evidence[premise]
			);
			if (evidence_status < 0) {
				return -1;
			}
			constraint->premise_states[premise] = evidence_status == 0 ?
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED :
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
	}
	return 0;
}

static int bind_cbpv_operation_classifiers(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	/* RETURN/THUNK/FORCE are one kernel rule parameterized by the child
	 * classifier of this source occurrence.  A shared erased boundary node may
	 * legitimately have several such derivations, so do not recover one child
	 * classifier globally from TermDB. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t child_occurrence;
		int child_role = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
			PROTOTYPE_TERM_CHILD_RETURN_VALUE :
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
				PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
				PROTOTYPE_TERM_CHILD_FORCE_VALUE);
		if ((operation->tag != PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_FORCE) ||
			compile_occurrence_child(
				ctx, operation_id, child_role, 0, &child_occurrence
			) != 0) {
			continue;
		}
		uint32_t child_classifier = operation_solver_classifier(
			ctx, child_occurrence
		);
		if (child_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		int boundary_status = prototype_judgement_cbpv_boundary_classifier(
				ctx->terms,
				ctx->type_declarations,
				operation->core_term,
				child_classifier,
				&classifier
			);
		if (boundary_status != 0) {
			continue;
		}
		if (operation_solver_bind_proven_classifier(
				ctx, operation_id, classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	return 0;
}
