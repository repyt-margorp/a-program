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
				ctx->terms,
				ctx->type_declarations,
				closed,
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
	if (!ctx || !ctx->metadata ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		result_row >= ctx->terms->term_count ||
		left_row >= ctx->terms->term_count ||
		(right_row != PROTOTYPE_INVALID_ID &&
			right_row >= ctx->terms->term_count) ||
		(origin_constraint_id != PROTOTYPE_INVALID_ID &&
			(origin_constraint_id >= ctx->constraints.constraint_count ||
			 ctx->constraints.constraints[origin_constraint_id].domain !=
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION))) {
		return -1;
	}
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW) {
			return -1;
		}
		if (constraint->kind == kind &&
			constraint->source_occurrence == operation &&
			constraint->origin_constraint_id == origin_constraint_id &&
			constraint->payload.effect.result_row == result_row &&
			constraint->payload.effect.left_row == left_row &&
			constraint->payload.effect.right_row == right_row) {
			return 0;
		}
	}
	if (ctx->constraints.effect_constraint_count >=
			PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY ||
		ctx->constraints.constraint_count >=
			PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->constraints.constraint_count++;
	ctx->constraints.effect_constraint_count++;
	ctx->constraints.constraints[id] = (struct operation_constraint){
		.id = id,
		.domain = OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW,
		.kind = kind,
		.state = PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_PENDING,
		.evidence_id = PROTOTYPE_INVALID_ID,
		.origin_constraint_id = origin_constraint_id,
		.source_occurrence = operation,
		.source_ast = ctx->metadata->typed_occurrences.occurrences[
			operation
		].source_ast,
		.context_id = ctx->metadata->typed_occurrences.occurrences[
			operation
		].context_id,
		.payload.effect = {
			.effect_meta_id = PROTOTYPE_INVALID_ID,
			.result_row = result_row,
			.left_row = left_row,
			.right_row = right_row
		}
	};
	return 0;
}

static int operation_classifier_computation_view(
	struct compile_context* ctx,
	uint32_t operation,
	struct prototype_term_classifier_view* p_view
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
	return p_view->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		p_view->computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		p_view->effect_row != PROTOTYPE_INVALID_ID ? 0 : 1;
}

static int operation_effect_row_meta_find_or_add(
	struct compile_context* ctx,
	uint32_t owner_occurrence,
	uint32_t placeholder_row,
	struct operation_effect_row_meta** p_meta
) {
	if (!ctx || !p_meta || owner_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
		placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[placeholder_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t existing = ctx->effect_solver.meta_for_occurrence[owner_occurrence];
	if (existing != PROTOTYPE_INVALID_ID) {
		if (existing >= ctx->effect_solver.meta_count ||
			ctx->effect_solver.metas[existing].placeholder_row != placeholder_row) {
			return -1;
		}
		*p_meta = &ctx->effect_solver.metas[existing];
		return 0;
	}
	if (ctx->effect_solver.meta_count >=
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_META_CAPACITY) {
		return -1;
	}
	uint32_t meta_id = ctx->effect_solver.meta_count++;
	struct operation_effect_row_meta* meta =
		&ctx->effect_solver.metas[meta_id];
	*meta = (struct operation_effect_row_meta){
		.owner_occurrence = owner_occurrence,
		.placeholder_row = placeholder_row,
		.first_solution_atom = PROTOTYPE_INVALID_ID,
		.materialized_row = PROTOTYPE_INVALID_ID,
		.evidence_constraint_id = PROTOTYPE_INVALID_ID,
		.state = OPERATION_EFFECT_ROW_META_UNSOLVED
	};
	ctx->effect_solver.meta_for_occurrence[owner_occurrence] = meta_id;
	*p_meta = meta;
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

static int operation_effect_meta_solution_normal(
	const struct operation_effect_solver* solver,
	const struct operation_effect_row_meta* meta,
	struct prototype_effect_row_normal_form* p_normal
) {
	if (!solver || !meta || !p_normal ||
		meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
		meta->first_solution_atom > solver->solution_atom_count ||
		meta->solution_atom_count > solver->solution_atom_count -
			meta->first_solution_atom ||
		meta->solution_atom_count >
			PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	p_normal->atom_count = meta->solution_atom_count;
	memcpy(
		p_normal->atoms,
		&solver->solution_atoms[meta->first_solution_atom],
		meta->solution_atom_count * sizeof(*p_normal->atoms)
	);
	return 0;
}

static int operation_effect_meta_store_solution(
	struct compile_context* ctx,
	struct operation_effect_solver* solver,
	struct operation_effect_row_meta* meta,
	const struct prototype_effect_row_normal_form* normal,
	uint32_t evidence_constraint_id
) {
	if (!ctx || !solver || !meta || !normal ||
		normal->atom_count > PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_SOLUTION_ATOM_CAPACITY -
			solver->solution_atom_count) {
		return -1;
	}
	if (meta->state == OPERATION_EFFECT_ROW_META_SOLVED) {
		struct prototype_effect_row_normal_form existing;
		return operation_effect_meta_solution_normal(
			solver, meta, &existing
		) == 0 && operation_effect_normal_equal(
			ctx->terms, &existing, normal
		) ? 0 : -1;
	}
	meta->first_solution_atom = solver->solution_atom_count;
	meta->solution_atom_count = normal->atom_count;
	memcpy(
		&solver->solution_atoms[solver->solution_atom_count],
		normal->atoms,
		normal->atom_count * sizeof(*normal->atoms)
	);
	solver->solution_atom_count += normal->atom_count;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
	meta->evidence_constraint_id = evidence_constraint_id;
	return 0;
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

static int operation_effect_resolve_row_normal(
	struct compile_context* ctx,
	uint32_t owner_occurrence,
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
		struct operation_effect_row_meta* solved_meta = NULL;
		if (atom < ctx->terms->term_count &&
			ctx->terms->terms[atom].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			uint32_t meta_id = ctx->effect_solver.meta_for_occurrence[owner_occurrence];
			if (meta_id != PROTOTYPE_INVALID_ID &&
				meta_id < ctx->effect_solver.meta_count &&
				ctx->effect_solver.metas[meta_id].placeholder_row == atom &&
				ctx->effect_solver.metas[meta_id].state ==
					OPERATION_EFFECT_ROW_META_SOLVED) {
				solved_meta = &ctx->effect_solver.metas[meta_id];
			}
		}
		if (solved_meta) {
			struct prototype_effect_row_normal_form solution;
			if (operation_effect_meta_solution_normal(
					&ctx->effect_solver, solved_meta, &solution
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

static int operation_effect_materialize_solution(
	struct compile_context* ctx,
	struct operation_effect_row_meta* meta,
	uint32_t replacement
) {
	if (!ctx || !meta || replacement >= ctx->terms->term_count ||
		meta->owner_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
		meta->placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[meta->placeholder_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t owner = meta->owner_occurrence;
	uint32_t binding_id = ctx->terms->terms[
		meta->placeholder_row
	].as.effect_row_var.binding_id;
	uint32_t classifier =
		ctx->classifier_solver.classifier_metas[owner].classifier;
	if (classifier != PROTOTYPE_INVALID_ID) {
		uint32_t rewritten;
		uint32_t canonical;
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms,
				ctx->type_declarations,
				classifier,
				binding_id,
				replacement,
				&rewritten
			) != 0 || operation_classifier_canonicalize_effect_rows(
				ctx, rewritten, 0, &canonical
			) != 0 || operation_classifier_meta_publish(
				ctx,
				owner,
				canonical,
				ctx->classifier_solver.classifier_metas[owner].
					evidence_constraint_id
			) != 0) {
			return -1;
		}
	}
	meta->materialized_row = replacement;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
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

static int operation_effect_solve_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
			uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
			if (id >= ctx->constraints.constraint_count) {
				return -1;
			}
			struct operation_constraint* constraint =
				&ctx->constraints.constraints[id];
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW) {
				return -1;
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
				if (operation_constraint_transition(
						constraint,
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
					constraint->source_occurrence,
					constraint->payload.effect.left_row,
					&left
				) != 0 ||
				(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
				 operation_effect_resolve_row_normal(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.right_row,
					&right
				 ) != 0) ||
				operation_effect_resolve_row_normal(
					ctx,
					constraint->source_occurrence,
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
				if (operation_constraint_transition(
						constraint,
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
				if (operation_constraint_transition(
						constraint,
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
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			if (!operation_effect_normal_is_ground(ctx->terms, &expected)) {
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			struct operation_effect_row_meta* meta;
			if (operation_effect_row_meta_find_or_add(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.result_row,
					&meta
				) != 0) {
				return -1;
			}
			int was_unsolved = meta->state == OPERATION_EFFECT_ROW_META_UNSOLVED;
			if (operation_effect_meta_store_solution(
					ctx, &ctx->effect_solver, meta, &expected, constraint->id
				) != 0) {
				return -1;
			}
			constraint->payload.effect.effect_meta_id =
				(uint32_t)(meta - ctx->effect_solver.metas);
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_SOLVED,
					OPERATION_CLASSIFIER_GOAL_REASON_NONE
				) != 0) {
				return -1;
			}
			if (was_unsolved) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->state == OPERATION_CONSTRAINT_STATE_PENDING &&
			operation_constraint_transition(
				constraint,
				OPERATION_CONSTRAINT_STATE_RESIDUAL,
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
			meta->materialized_row != PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_effect_row_normal_form solution;
		uint32_t replacement;
		if (operation_effect_meta_solution_normal(
				&ctx->effect_solver, meta, &solution
			) != 0 || prototype_term_effect_row_materialize_normal_form(
				ctx->terms, &solution, &replacement
			) != 0 || operation_effect_materialize_solution(
				ctx, meta, replacement
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
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->effect_constraint_summary.solved_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->effect_constraint_summary.residual_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
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
	if (computation->subject != source->subject ||
		computation->occurrence_id == PROTOTYPE_INVALID_ID ||
		computation->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t lifecycle_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < ctx->constraints.computation_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_computation_constraint + i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* candidate =
			&ctx->constraints.constraints[id];
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
	struct prototype_term_classifier_view current_view;
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
	/* These constraints are a derived view of the current operation
	 * classifiers. A fixed-point round may replace a symbolic row with a
	 * concrete one, so retaining constraints from an earlier round creates
	 * false residual obligations. */
	uint32_t empty_row;
	if (prototype_term_effect_row_empty(ctx->terms, &empty_row
		) != 0) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		ctx->classifier_solver.initialized &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		struct prototype_term_classifier_view result_view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &result_view
			) != 0) {
			continue;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN) {
			if (operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT,
					operation_id,
					result_view.effect_row,
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
			struct prototype_term_classifier_view child_view;
			if (operation_classifier_computation_view(
					ctx, child, &child_view
				) == 0 && operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					child_view.effect_row,
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
				struct prototype_term_classifier_view case_view;
				if (operation_classifier_computation_view(
						ctx, branch_operation, &case_view
					) != 0) {
					accumulated = PROTOTYPE_INVALID_ID;
					break;
				}
				if (accumulated == PROTOTYPE_INVALID_ID) {
					accumulated = case_view.effect_row;
					continue;
				}
				uint32_t union_row;
				if (prototype_term_effect_row_union(
						ctx->terms,
						accumulated,
						case_view.effect_row,
						&union_row
					) != 0 || operation_effect_constraint_add(
						ctx,
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION,
						operation_id,
						union_row,
						accumulated,
						case_view.effect_row,
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
					result_view.effect_row,
					accumulated,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		}
		int has_constraint = 0;
		for (size_t constraint_id = 0;
			constraint_id < ctx->constraints.effect_constraint_count;
			++constraint_id) {
			uint32_t id = ctx->constraints.first_effect_constraint +
				(uint32_t)constraint_id;
			if (id >= ctx->constraints.constraint_count) {
				return -1;
			}
			if (ctx->constraints.constraints[id].source_occurrence == operation_id) {
				has_constraint = 1;
				break;
			}
		}
		if (!has_constraint && operation_effect_constraint_add(
				ctx,
				PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
				operation_id,
				result_view.effect_row,
				result_view.effect_row,
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
	return operation_effect_solve_constraints(ctx);
}

static int operation_effect_generate_constraints(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	ctx->constraints.constraint_count = ctx->constraints.first_effect_constraint;
	ctx->constraints.effect_constraint_count = 0;
	struct operation_constraint_db_mark mark =
		operation_constraint_db_mark(&ctx->constraints);
	int status = operation_effect_generate_constraints_impl(ctx);
	if (status != 0 && operation_constraint_db_rollback(
			&ctx->constraints, mark
		) != 0) {
		return -1;
	}
	return status;
}

static size_t operation_effect_unresolved_count(
	const struct operation_constraint_db* constraints
) {
	if (!constraints) {
		return 0;
	}
	size_t count = 0;
	for (size_t i = 0; i < constraints->effect_constraint_count; ++i) {
		uint32_t id = constraints->first_effect_constraint + (uint32_t)i;
		if (id >= constraints->constraint_count ||
			constraints->constraints[id].state !=
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
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
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->state ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
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
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t constraint_id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[constraint_id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->state != OPERATION_CONSTRAINT_STATE_RESIDUAL ||
			constraint->payload.branch_refinement.refinement_status !=
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
			constraint->payload.branch_refinement.residual_pattern,
			constraint->payload.branch_refinement.residual_value,
			source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[source_ast].span :
				(struct prototype_source_span) { 0, 0 }
		);
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		switch (operation->tag) {
			case PROTOTYPE_TYPED_OCCURRENCE_APP:
			case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
				if (ctx->classifier_solver.classifier_metas[i].classifier ==
					PROTOTYPE_INVALID_ID) {
					fprintf(stderr,
						"classifier solver incomplete occurrence=%u tag=%d ast=%u context=%u core=%u action=%u ih-owner=%u ih-case=%u ih-field=%u scope=%u\n",
						i, operation->tag, operation->source_ast,
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
						const struct prototype_typed_occurrence* owner_occurrence =
							&ctx->metadata->typed_occurrences.occurrences[owner];
						fprintf(stderr,
							"  IH owner tag=%d ast=%u context=%u core=%u classifier=%u motive=%u motive-state=%u\n",
							owner_occurrence->tag, owner_occurrence->source_ast,
							owner_occurrence->context_id, owner_occurrence->core_term,
							ctx->classifier_solver.classifier_metas[owner].classifier,
							ctx->classifier_solver.motive_terms[owner],
							ctx->classifier_solver.motive_solution_states[owner]);
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
								ctx->classifier_solver.classifier_metas[child].classifier :
								PROTOTYPE_INVALID_ID);
						if (child < ctx->metadata->typed_occurrences.occurrence_count &&
							ctx->classifier_solver.classifier_metas[child].classifier !=
								PROTOTYPE_INVALID_ID) {
							fprintf(stderr, "    classifier-term: ");
							prototype_term_print_debug(
								stderr, ctx->symbols, ctx->intrinsic_environment,
								ctx->type_declarations, ctx->terms,
								ctx->classifier_solver.classifier_metas[child].classifier
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
							ctx->classifier_solver.classifier_metas[debug].classifier);
						prototype_term_print_debug(
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
						for (uint32_t j = 0;
							j < ctx->constraints.classifier_constraint_count;
							++j) {
							const struct operation_constraint* constraint =
								&ctx->constraints.constraints[j];
							if (constraint->source_occurrence != i) {
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
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MATCH_RESULT_MOTIVE;
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
						(void)add_compile_diagnostic(
							ctx,
							PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
							reason,
							operation->source_ast,
							i,
							constraint_id,
							expected,
							actual,
							operation->source_ast < ctx->asts->node_count ?
								ctx->asts->nodes[operation->source_ast].span :
								(struct prototype_source_span) { 0, 0 }
						);
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
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t continuation_occurrence;
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
		uint32_t input_classifier = operation_solver_classifier(
			ctx, computation_occurrence
		);
		uint32_t continuation_classifier = operation_solver_classifier(
			ctx, continuation_occurrence
		);
		struct prototype_term_classifier_view input_view;
		uint32_t domain;
		uint32_t classifier_family;
		uint32_t continuation_binder_id;
		uint32_t codomain;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			continuation_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
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
		struct prototype_term_normalization_result normalized;
		uint64_t normalization_limit = compile_effort_remaining(ctx);
		int effort_status = compile_effort_charge(
			ctx, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
		);
		if (effort_status != 0) return effort_status;
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				ctx->terms->terms[operation->core_term].as.computation_fold.computation,
				normalization_limit,
				&normalized
			) != 0 || normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
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
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
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
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t return_operation;
		uint32_t return_body_occurrence;
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
		uint32_t input_classifier = operation_solver_classifier(
			ctx, computation_occurrence
		);
		uint32_t return_classifier = operation_solver_classifier(
			ctx, return_body_occurrence
		);
		struct prototype_term_classifier_view input_view;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			return_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!prototype_term_contains_free_binding(
				ctx->terms, return_classifier, operation->fold_return_binder_id
			)) {
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
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
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
