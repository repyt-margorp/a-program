static int operation_solver_generate_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->operation_count > 4096) {
		return -1;
	}
	memset(&ctx->classifier_solver, 0, sizeof(ctx->classifier_solver));
	for (uint32_t i = 0; i < 4096; ++i) {
		ctx->classifier_solver.solutions[i].classifier = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.solutions[i].effect_row = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.solutions[i].first_usage_entry = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.solutions[i].usage_entry_count = 0;
		ctx->classifier_solver.solutions[i].binder_usage = PROTOTYPE_USAGE_ZERO;
		ctx->classifier_solver.solutions[i].classifier_state =
			OPERATION_CONSTRAINT_STATE_PENDING;
		ctx->classifier_solver.solutions[i].effect_state =
			OPERATION_CONSTRAINT_STATE_PENDING;
		ctx->classifier_solver.solutions[i].usage_state =
			OPERATION_CONSTRAINT_STATE_PENDING;
		ctx->classifier_solver.solutions[i].reason =
			OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		ctx->classifier_solver.motive_constant_candidates[i] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_solution_states[i] =
			OPERATION_MOTIVE_SOLUTION_UNRESOLVED;
		ctx->classifier_solver.ih_motive_application_ids[i] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_terms[i] = PROTOTYPE_INVALID_ID;
	}
	if (operation_solver_refresh_usage_solutions(ctx) != 0 ||
		operation_solver_initialize_input_facts(ctx) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		int base_constraint_kind = OPERATION_CONSTRAINT_HAS_TYPE;
		union operation_classifier_goal_payload base_payload = { 0 };
		if (operation->tag == PROTOTYPE_OPERATION_APP &&
			operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			base_constraint_kind = OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION;
			base_payload.constructor_formation.function_operation = operation->function;
			base_payload.constructor_formation.argument_operation = operation->argument;
		} else if (operation->tag == PROTOTYPE_OPERATION_RETURN ||
			operation->tag == PROTOTYPE_OPERATION_THUNK ||
			operation->tag == PROTOTYPE_OPERATION_FORCE) {
			base_constraint_kind = OPERATION_CONSTRAINT_CBPV_BOUNDARY;
			base_payload.cbpv_boundary.child_operation = operation->argument;
		} else if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
			base_constraint_kind = OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT;
			base_payload.computation.first_operation = operation->function;
			base_payload.computation.second_operation = operation->argument;
			base_payload.computation.third_operation = operation->scrutinee;
		} else if (operation->tag == PROTOTYPE_OPERATION_REQUEST) {
			base_constraint_kind = OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT;
			base_payload.computation.first_operation = operation->function;
			base_payload.computation.second_operation = operation->argument;
			base_payload.computation.third_operation = operation->body;
		}
		if (operation->classifier_variable != i ||
			operation_solver_add_classifier_goal(
				ctx,
				base_constraint_kind,
				i,
				base_payload
			) != 0) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.reference.referenced_operation = operation->function;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_EQUAL, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.conversion.body_operation = operation->body;
			payload.conversion.expected_classifier = operation->known_classifier;
			if (operation->body >= ctx->metadata->operation_count ||
				operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_CONVERTIBLE, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_LAMBDA) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_INTRO;
			payload.pi.body_or_function_operation = operation->body;
			payload.pi.domain_classifier_or_argument_operation =
				operation->binder_classifier;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_APP &&
			operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_ELIM;
			payload.pi.body_or_function_operation = operation->function;
			payload.pi.domain_classifier_or_argument_operation = operation->argument;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
			for (uint32_t j = 0; j < operation->case_count; ++j) {
				if (operation->first_case + j >= ctx->metadata->operation_case_count ||
					operation_solver_add_motive_case_goal(
						ctx,
						i,
						j,
						ctx->metadata->operation_cases[operation->first_case + j].body_operation,
						operation->scrutinee
					) != 0) {
					return -1;
				}
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.induction_hypothesis.recursive_argument_operation =
				operation->argument;
			payload.induction_hypothesis.ih_scope_id = operation->first_case;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_IH_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		}
	}
	return operation_solver_index_constraints(ctx);
}

static int operation_solver_seed_known_classifiers(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &ctx->metadata->operations[i];
		uint32_t seed = operation->known_classifier != PROTOTYPE_INVALID_ID ?
			operation->known_classifier : operation->classifier;
		/*
		 * ASCRIPTION is solved against its body, and NAME follows its referenced
		 * source operation. Seeding either from the surface classifier creates a
		 * second authoritative path and can retain unsolved elaboration rows.
		 */
		if (operation->tag != PROTOTYPE_OPERATION_ASCRIPTION &&
			operation->tag != PROTOTYPE_OPERATION_NAME &&
			seed != PROTOTYPE_INVALID_ID &&
			operation_solver_bind(ctx, i, seed, p_changed) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_commit_bindings(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		uint32_t classifier = ctx->classifier_solver.solutions[i].classifier;
		if (classifier != PROTOTYPE_INVALID_ID &&
			ctx->metadata->operations[i].classifier != classifier) {
			ctx->metadata->operations[i].classifier = classifier;
			*p_changed = 1;
		}
	}
	return 0;
}

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
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t owner = 0; owner < ctx->metadata->operation_count; ++owner) {
		const struct prototype_operation_node* lambda =
			&ctx->metadata->operations[owner];
		if (lambda->tag != PROTOTYPE_OPERATION_LAMBDA) {
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
		uint8_t visited[ctx->metadata->operation_count];
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
	}
	return 0;
}

static int operation_classifier_contains_unowned_effect_row(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier
) {
	if (!ctx || !ctx->terms || !ctx->metadata ||
		operation_id >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)ctx->terms->term_count; ++i) {
		if (ctx->terms->terms[i].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t binding_id = ctx->terms->terms[i].as.effect_row_var.binding_id;
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
	uint32_t right_row
) {
	if (!ctx || !ctx->metadata || !ctx->metadata->effect_constraints ||
		operation >= ctx->metadata->operation_count ||
		result_row >= ctx->terms->term_count ||
		left_row >= ctx->terms->term_count ||
		(right_row != PROTOTYPE_INVALID_ID &&
			right_row >= ctx->terms->term_count)) {
		return -1;
	}
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		const struct prototype_operation_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (constraint->kind == kind &&
			constraint->operation == operation &&
			constraint->result_row == result_row &&
			constraint->left_row == left_row &&
			constraint->right_row == right_row) {
			return 0;
		}
	}
	if (ctx->metadata->effect_constraint_count >=
		ctx->metadata->effect_constraint_capacity) {
		return -1;
	}
	ctx->metadata->effect_constraints[
		ctx->metadata->effect_constraint_count++
	] = (struct prototype_operation_effect_constraint){
		.kind = kind,
		.state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING,
		.operation = operation,
		.result_row = result_row,
		.left_row = left_row,
		.right_row = right_row
	};
	ctx->effect_solver.constraint_states[
		ctx->metadata->effect_constraint_count - 1
	] = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING;
	return 0;
}

static int operation_classifier_computation_view(
	struct compile_context* ctx,
	uint32_t operation,
	struct prototype_term_classifier_view* p_view
) {
	if (!ctx || !p_view || operation >= ctx->metadata->operation_count) {
		return -1;
	}
	/* Effect equations validate the selected OperationGraph classifier. The
	 * classifier solver work cell may temporarily be ahead of or behind the
	 * committed occurrence during another fixed-point pass. */
	uint32_t classifier = ctx->metadata->operations[operation].classifier;
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
	uint32_t owner_operation,
	uint32_t placeholder_row,
	struct operation_effect_row_meta** p_meta
) {
	if (!ctx || !p_meta || owner_operation >= ctx->metadata->operation_count ||
		placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[placeholder_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->owner_operation == owner_operation &&
			meta->placeholder_row == placeholder_row) {
			*p_meta = meta;
			return 0;
		}
	}
	if (ctx->effect_solver.meta_count >=
		PROTOTYPE_OPERATION_EFFECT_ROW_META_CAPACITY) {
		return -1;
	}
	struct operation_effect_row_meta* meta =
		&ctx->effect_solver.metas[ctx->effect_solver.meta_count++];
	*meta = (struct operation_effect_row_meta){
		.owner_operation = owner_operation,
		.placeholder_row = placeholder_row,
		.first_solution_atom = PROTOTYPE_INVALID_ID,
		.solution_row = PROTOTYPE_INVALID_ID,
		.state = OPERATION_EFFECT_ROW_META_UNSOLVED
	};
	*p_meta = meta;
	return 0;
}

static int operation_effect_normal_add_atom(
	struct prototype_effect_row_normal_form* normal,
	uint32_t atom
) {
	if (!normal) {
		return -1;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (normal->atoms[i] == atom) {
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
	struct prototype_effect_row_normal_form* target,
	const struct prototype_effect_row_normal_form* source
) {
	if (!target || !source) {
		return -1;
	}
	for (uint32_t i = 0; i < source->atom_count; ++i) {
		if (operation_effect_normal_add_atom(target, source->atoms[i]) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_normal_equal(
	const struct prototype_effect_row_normal_form* left,
	const struct prototype_effect_row_normal_form* right
) {
	return left && right &&
		prototype_term_effect_row_normal_form_includes(left, right) &&
		prototype_term_effect_row_normal_form_includes(right, left);
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
	struct operation_effect_solver* solver,
	struct operation_effect_row_meta* meta,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!solver || !meta || !normal ||
		normal->atom_count > PROTOTYPE_OPERATION_EFFECT_ROW_SOLUTION_ATOM_CAPACITY -
			solver->solution_atom_count) {
		return -1;
	}
	if (meta->state == OPERATION_EFFECT_ROW_META_SOLVED) {
		struct prototype_effect_row_normal_form existing;
		return operation_effect_meta_solution_normal(
			solver, meta, &existing
		) == 0 && operation_effect_normal_equal(&existing, normal) ? 0 : -1;
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
	return 0;
}

static int operation_effect_resolve_row_normal(
	struct compile_context* ctx,
	uint32_t owner_operation,
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
			for (uint32_t j = 0; j < ctx->effect_solver.meta_count; ++j) {
				struct operation_effect_row_meta* candidate =
					&ctx->effect_solver.metas[j];
				if (candidate->owner_operation == owner_operation &&
					candidate->placeholder_row == atom &&
					candidate->state == OPERATION_EFFECT_ROW_META_SOLVED) {
					solved_meta = candidate;
					break;
				}
			}
		}
		if (solved_meta) {
			struct prototype_effect_row_normal_form solution;
			if (operation_effect_meta_solution_normal(
					&ctx->effect_solver, solved_meta, &solution
				) != 0 || operation_effect_normal_union(p_normal, &solution) != 0) {
				return -1;
			}
		} else if (operation_effect_normal_add_atom(p_normal, atom) != 0) {
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
		meta->owner_operation >= ctx->metadata->operation_count ||
		meta->placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[meta->placeholder_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t owner = meta->owner_operation;
	uint32_t binding_id = ctx->terms->terms[
		meta->placeholder_row
	].as.effect_row_var.binding_id;
	uint32_t* classifiers[] = {
		&ctx->metadata->operations[owner].known_classifier,
		&ctx->metadata->operations[owner].classifier,
		&ctx->metadata->operations[owner].binder_classifier,
		&ctx->classifier_solver.solutions[owner].classifier
	};
	for (size_t i = 0; i < sizeof(classifiers) / sizeof(classifiers[0]); ++i) {
		if (*classifiers[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms,
				ctx->type_declarations,
				*classifiers[i],
				binding_id,
				replacement,
				classifiers[i]
			) != 0) {
			return -1;
		}
	}
	meta->solution_row = replacement;
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
		for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
			const struct prototype_operation_effect_constraint* constraint =
				&ctx->metadata->effect_constraints[i];
			int* state = &ctx->effect_solver.constraint_states[i];
			if ((constraint->kind ==
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT ||
				 constraint->kind ==
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY) &&
				constraint->result_row == constraint->left_row) {
				int unowned = operation_classifier_contains_unowned_effect_row(
					ctx,
					constraint->operation,
					ctx->metadata->operations[constraint->operation].classifier
				);
				if (unowned < 0) {
					return -1;
				}
				int has_external_ref = unowned != 0 ?
					operation_subtree_contains_external_ref(
						ctx, constraint->operation
					) : 0;
				if (has_external_ref < 0) {
					return -1;
				}
				/* The identity equation adds no obligation when every symbolic
				 * row is scoped by this Operation. A free external row remains a
				 * residual link-time obligation even though rho = rho is trivial. */
				*state = unowned != 0 && has_external_ref != 0 ?
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL :
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
				continue;
			}
			struct prototype_effect_row_normal_form left;
			struct prototype_effect_row_normal_form right;
			struct prototype_effect_row_normal_form expected;
			struct prototype_effect_row_normal_form actual;
			if (operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->left_row, &left
				) != 0 ||
				(constraint->right_row != PROTOTYPE_INVALID_ID &&
				 operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->right_row, &right
				 ) != 0) ||
				operation_effect_resolve_row_normal(
					ctx, constraint->operation, constraint->result_row, &actual
				) != 0) {
				return -1;
			}
			memset(&expected, 0, sizeof(expected));
			switch (constraint->kind) {
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT:
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY:
					expected = left;
					break;
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION:
					expected = left;
					if (operation_effect_normal_union(&expected, &right) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_RESIDUAL:
					for (uint32_t handled_index = 0;
						handled_index < right.atom_count;
						++handled_index) {
						uint32_t handled_atom = right.atoms[handled_index];
						if (handled_atom >= ctx->terms->term_count ||
							ctx->terms->terms[handled_atom].tag !=
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							*state =
								PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
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
								&expected, atom
							) != 0) {
							return -1;
						}
					}
				residual_complete:
					if (*state ==
						PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL) {
						continue;
					}
					break;
				default:
					return -1;
			}
			if (operation_effect_normal_equal(&actual, &expected)) {
				*state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
				continue;
			}
			if (constraint->result_row >= ctx->terms->term_count ||
				ctx->terms->terms[constraint->result_row].tag !=
					PROTOTYPE_TERM_EFFECT_ROW_VAR) {
				if (operation_effect_normal_is_ground(ctx->terms, &actual) &&
					operation_effect_normal_is_ground(ctx->terms, &expected)) {
					return -1;
					}
				*state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			uint32_t result_binder = ctx->terms->terms[
				constraint->result_row
			].as.effect_row_var.binding_id;
			int owned = operation_effect_row_binder_is_owned(
				ctx, constraint->operation, result_binder
			);
			if (owned < 0) {
				return -1;
			}
			if (owned) {
				*state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			if (!operation_effect_normal_is_ground(ctx->terms, &expected)) {
				*state =
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL;
				continue;
			}
			struct operation_effect_row_meta* meta;
			if (operation_effect_row_meta_find_or_add(
					ctx, constraint->operation, constraint->result_row, &meta
				) != 0) {
				return -1;
			}
			int was_unsolved = meta->state == OPERATION_EFFECT_ROW_META_UNSOLVED;
			if (operation_effect_meta_store_solution(
					&ctx->effect_solver, meta, &expected
				) != 0) {
				return -1;
			}
			*state = PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED;
			if (was_unsolved) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
			meta->solution_row != PROTOTYPE_INVALID_ID) {
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
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		struct prototype_operation_effect_constraint* projection =
			&ctx->metadata->effect_constraints[i];
		projection->state = ctx->effect_solver.constraint_states[i];
		if (projection->operation < ctx->metadata->operation_count) {
			struct operation_solver_solution* solution =
				&ctx->classifier_solver.solutions[projection->operation];
			solution->effect_state = projection->state;
			if (projection->state ==
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
				solution->effect_row = projection->result_row;
			}
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
	int imported = 0;
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->core_term != source->subject) {
			continue;
		}
		struct prototype_term_classifier_view view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &view
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				view.effect_row,
				source->result_row
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			continue;
		}
		int kind = source->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN ?
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION :
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_RESIDUAL;
		if (operation_effect_constraint_add(
				ctx,
				kind,
				operation_id,
				source->result_row,
				source->operands[0],
				source->operands[1]
			) != 0) {
			return -1;
		}
		imported = 1;
	}
	/* Authority-neutral helper equations can describe a shared Core subject
	 * whose current typed occurrences own a different solved row. They remain
	 * JudgementDelta-local and are deliberately not promoted to an occurrence
	 * equation. */
	(void)imported;
	return 0;
}

static int operation_effect_generate_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* These constraints are a derived view of the current operation
	 * classifiers. A fixed-point round may replace a symbolic row with a
	 * concrete one, so retaining constraints from an earlier round creates
	 * false residual obligations. */
	ctx->metadata->effect_constraint_count = 0;
	uint32_t empty_row;
	if (prototype_term_effect_row_empty(ctx->terms, &empty_row
		) != 0) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		struct prototype_term_classifier_view result_view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &result_view
			) != 0) {
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_RETURN) {
			if (operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT,
					operation_id,
					result_view.effect_row,
					empty_row,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_NAME ||
			operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ||
			operation->tag == PROTOTYPE_OPERATION_FORCE) {
			uint32_t child = operation->tag == PROTOTYPE_OPERATION_NAME ?
				operation->function :
				(operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ?
					operation->body : operation->argument);
			struct prototype_term_classifier_view child_view;
			if (operation_classifier_computation_view(
					ctx, child, &child_view
				) == 0 && operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					child_view.effect_row,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
			operation->case_count != 0) {
			uint32_t accumulated = PROTOTYPE_INVALID_ID;
			for (uint32_t case_index = 0;
				case_index < operation->case_count;
				++case_index) {
				uint32_t graph_case = operation->first_case + case_index;
				if (graph_case >= ctx->metadata->operation_case_count) {
					return -1;
				}
				struct prototype_term_classifier_view case_view;
				if (operation_classifier_computation_view(
						ctx,
						ctx->metadata->operation_cases[
							graph_case
						].body_operation,
						&case_view
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
						PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION,
						operation_id,
						union_row,
						accumulated,
						case_view.effect_row
					) != 0) {
					return -1;
				}
				accumulated = union_row;
			}
			if (accumulated != PROTOTYPE_INVALID_ID &&
				operation_effect_constraint_add(
					ctx,
					PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					accumulated,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		}
		int has_constraint = 0;
		for (size_t constraint_id = 0;
			constraint_id < ctx->metadata->effect_constraint_count;
			++constraint_id) {
			if (ctx->metadata->effect_constraints[constraint_id].operation ==
				operation_id) {
				has_constraint = 1;
				break;
			}
		}
		if (!has_constraint && operation_effect_constraint_add(
				ctx,
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY,
				operation_id,
				result_view.effect_row,
				result_view.effect_row,
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

static size_t operation_effect_unresolved_count(
	const struct prototype_compile_metadata* metadata
) {
	if (!metadata) {
		return 0;
	}
	size_t count = 0;
	for (size_t i = 0; i < metadata->effect_constraint_count; ++i) {
		if (metadata->effect_constraints[i].state !=
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
			count++;
		}
	}
	return count;
}

static int operation_solver_require_complete(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		switch (operation->tag) {
			case PROTOTYPE_OPERATION_APP:
			case PROTOTYPE_OPERATION_LAMBDA:
			case PROTOTYPE_OPERATION_MATCH:
			case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
				if (ctx->classifier_solver.solutions[i].classifier ==
					PROTOTYPE_INVALID_ID) {
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
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count != 0 ||
			operation->function >= ctx->metadata->operation_count ||
			operation->argument >= ctx->metadata->operation_count) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(ctx, operation->function);
		uint32_t continuation_classifier = operation_solver_classifier(
			ctx, operation->argument
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
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				ctx->terms->terms[operation->core_term].as.computation_fold.computation,
				ctx->metadata->normalization_step_limit,
				&normalized
			) != 0 || normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (UINT64_MAX - ctx->metadata->normalization_steps_used < normalized.steps_used) {
			return -1;
		}
		ctx->metadata->normalization_steps_used += normalized.steps_used;
		if (normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			continue;
		}
		if (normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT &&
			normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_operation(
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
					.operation = operation_id,
					.core_term = operation->core_term,
					.computation_operation = operation->function,
					.continuation_operation = operation->argument,
					.continuation_binder_id = continuation_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
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
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			operation->function >= ctx->metadata->operation_count ||
			operation->scrutinee >= ctx->metadata->operation_count ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(ctx, operation->function);
		uint32_t return_classifier = operation_solver_classifier(ctx, operation->scrutinee);
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
		int find_status = prototype_verification_db_find_operation(
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
					.operation = operation_id,
					.core_term = operation->core_term,
					.computation_operation = operation->function,
					.continuation_operation = operation->scrutinee,
					.continuation_binder_id =
						operation->fold_return_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
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
	if (!ctx || !ctx->metadata || !p_ret || operation_id >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
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

static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
);

static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_solver_classifier_goal_is_satisfied(
	const struct compile_context* ctx,
	const struct operation_classifier_constraint* goal,
	int* p_reason
) {
	if (!ctx || !ctx->metadata || !goal || !p_reason ||
		goal->classifier_variable >= ctx->metadata->operation_count ||
		goal->id >= ctx->classifier_solver.constraint_count) {
		return 0;
	}
	const struct operation_classifier_constraint_result* result =
		&ctx->classifier_solver.constraint_results[goal->id];
	uint32_t target = operation_solver_classifier(
		ctx, goal->classifier_variable
	);
	if (target == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_EQUAL: {
			uint32_t referenced = operation_solver_classifier(
				ctx, goal->payload.reference.referenced_operation
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
				PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (operation_solver_classifier(
					ctx, goal->payload.pi.body_or_function_operation
				) == PROTOTYPE_INVALID_ID ||
				(goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				 operation_solver_classifier(
					ctx,
					goal->payload.pi.domain_classifier_or_argument_operation
				 ) == PROTOTYPE_INVALID_ID)) {
				return 0;
			}
			*p_reason = goal->payload.pi.role ==
				OPERATION_CLASSIFIER_PI_GOAL_INTRO ?
				OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED :
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			if (operation_solver_classifier(
					ctx,
					goal->payload.constructor_formation.function_operation
				) == PROTOTYPE_INVALID_ID || operation_solver_classifier(
					ctx,
					goal->payload.constructor_formation.argument_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
			/* A Match may already carry a validated expected classifier, in which
			 * case no synthesized motive term is materialized. The semantic case
			 * goal is discharged once both that Match classifier and this branch
			 * classifier exist; JudgementDelta later validates the exact rule. */
			if (operation_solver_classifier(
					ctx, goal->payload.motive_case.branch_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_IH_EXPECTED:
			if (operation_solver_classifier(
					ctx,
					goal->payload.induction_hypothesis.recursive_argument_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			if (operation_solver_classifier(
					ctx, goal->payload.cbpv_boundary.child_operation
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
	ctx->metadata->solver_constraint_count = ctx->classifier_solver.constraint_count;
	ctx->metadata->solver_solved_count = 0;
	ctx->metadata->solver_residual_count = 0;
	ctx->metadata->solver_incomplete_count = 0;
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[i];
		solution->classifier_state = solution->classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CONSTRAINT_STATE_PENDING :
			OPERATION_CONSTRAINT_STATE_SOLVED;
		solution->reason = solution->classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
			OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
	}
	for (uint32_t i = 0; i < ctx->classifier_solver.constraint_count; ++i) {
		const struct operation_classifier_constraint* constraint =
			&ctx->classifier_solver.constraints[i];
		struct operation_classifier_constraint_result* result =
			&ctx->classifier_solver.constraint_results[i];
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
			if (obligation->operation == constraint->classifier_variable) {
				residual = 1;
				break;
			}
		}
		int solved_reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		int solved = operation_solver_classifier_goal_is_satisfied(
			ctx, constraint, &solved_reason
		);
		result->state = residual ? OPERATION_CONSTRAINT_STATE_RESIDUAL :
			(solved ? OPERATION_CONSTRAINT_STATE_SOLVED :
			(incomplete || goal_incomplete ? OPERATION_CONSTRAINT_STATE_INCOMPLETE :
				OPERATION_CONSTRAINT_STATE_PENDING));
		result->reason = residual ?
			(constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE ?
				OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL :
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION) :
			(solved ? solved_reason :
			(incomplete || goal_incomplete ?
				OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET :
				OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY));
		struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[constraint->classifier_variable];
		if (result->state != OPERATION_CONSTRAINT_STATE_SOLVED) {
			solution->classifier_state = result->state;
			solution->reason = result->reason;
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
		ctx->metadata->effect_constraint_count;
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		struct prototype_operation_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (incomplete &&
			constraint->state == PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING) {
			constraint->state =
				PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_INCOMPLETE;
		}
		if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state ==
			PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
		if (constraint->operation < ctx->metadata->operation_count) {
			ctx->classifier_solver.solutions[
				constraint->operation
			].effect_state = constraint->state;
		}
	}
}

static int operation_solver_propagate_zero_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t sequence_fold_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		sequence_fold_operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* sequence_fold_operation =
		&ctx->metadata->operations[sequence_fold_operation_id];
	if (sequence_fold_operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
		sequence_fold_operation->function >= ctx->metadata->operation_count ||
		sequence_fold_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		ctx->terms->terms[sequence_fold_operation->core_term].as.computation_fold.clause_count != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, sequence_fold_operation->function
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	const struct prototype_term* sequence_fold_term =
		&ctx->terms->terms[sequence_fold_operation->core_term];
	if (sequence_fold_term->as.computation_fold.return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		sequence_fold_operation->argument >= ctx->metadata->operation_count ||
		ctx->metadata->operations[sequence_fold_operation->argument].tag !=
			PROTOTYPE_OPERATION_LAMBDA) {
		return -1;
	}
	struct prototype_operation_node* continuation_operation =
		&ctx->metadata->operations[sequence_fold_operation->argument];
	if (continuation_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
		continuation_operation->binder_classifier = input_view.result;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(
				ctx, sequence_fold_operation->argument
			) != 0) {
			return -1;
		}
	} else if (!(prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			continuation_operation->binder_classifier,
			input_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t binder_var;
	if (prototype_term_var(
			ctx->terms,
			ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].as.lambda.binding_id,
			&binder_var
		) != 0) {
		return -1;
	}
	uint8_t visited[ctx->metadata->operation_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		memset(visited, 0, sizeof(visited));
		int binder_matches =
			continuation_operation->referenced_ast_binder_id != PROTOTYPE_INVALID_ID ?
				operation->referenced_ast_binder_id ==
					continuation_operation->referenced_ast_binder_id :
				operation->core_term == binder_var;
		if (operation->tag == PROTOTYPE_OPERATION_VAR && binder_matches &&
			operation_subtree_contains_operation(
				ctx,
				sequence_fold_operation->argument,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
		}
	}
	uint32_t continuation_classifier = operation_solver_classifier(
		ctx, sequence_fold_operation->argument
	);
	if (continuation_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t sequence_fold_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			ctx->terms,
			ctx->type_declarations,
			sequence_fold_term->as.computation_fold.computation,
			input_classifier,
			continuation_classifier,
			&sequence_fold_classifier
		);
		if (status < 0 || (status == 0 && operation_solver_widen_computation_binding(
				ctx, sequence_fold_operation_id, sequence_fold_classifier, p_changed
			) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_propagate_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t handle_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		handle_operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* handle_operation =
		&ctx->metadata->operations[handle_operation_id];
	if (handle_operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
		handle_operation->function >= ctx->metadata->operation_count ||
		handle_operation->scrutinee >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, handle_operation->function
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint8_t visited[ctx->metadata->operation_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation->binder_classifier == PROTOTYPE_INVALID_ID) {
			operation->binder_classifier = input_view.result;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
				return -1;
			}
		}
		memset(visited, 0, sizeof(visited));
		if (operation->tag == PROTOTYPE_OPERATION_VAR &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation_subtree_contains_operation(
				ctx,
				handle_operation->scrutinee,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
		}
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
			ctx->metadata->operation_fold_clause_count) {
		return -1;
	}
	uint32_t return_body_classifier = operation_solver_classifier(
		ctx, handle_operation->scrutinee
	);
	struct prototype_term_classifier_view return_body_view;
	if (return_body_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			return_body_classifier,
			&return_body_view
		) != 0 || return_body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		return_body_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	uint32_t fold_effect_row = return_body_view.effect_row;
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
		const struct prototype_operation_computation_fold_clause* occurrence_clause =
			&ctx->metadata->operation_fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
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
		uint32_t clause_body_classifier = operation_solver_classifier(
			ctx, occurrence_clause->body_operation
		);
		if (clause_body_classifier == PROTOTYPE_INVALID_ID) {
			clause_bodies_complete = 0;
			continue;
		}
		struct prototype_term_classifier_view clause_body_view;
		uint32_t joined_row;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				clause_body_classifier,
				&clause_body_view
			) != 0 || clause_body_view.category !=
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			clause_body_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_union(
				ctx->terms,
				fold_effect_row,
				clause_body_view.effect_row,
				&joined_row
			) != 0) {
			return -1;
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
		const struct prototype_operation_computation_fold_clause* occurrence_clause =
			&ctx->metadata->operation_fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		if (declaration->resumption_multiplicity !=
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT) {
			if (occurrence_clause->clause_operation >=
					ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* outer_clause =
				&ctx->metadata->operations[occurrence_clause->clause_operation];
			if (outer_clause->tag != PROTOTYPE_OPERATION_LAMBDA ||
				outer_clause->body >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* continuation_clause =
				&ctx->metadata->operations[outer_clause->body];
			const struct operation_solver_solution* continuation_solution =
				&ctx->classifier_solver.solutions[outer_clause->body];
			if (continuation_clause->tag != PROTOTYPE_OPERATION_LAMBDA ||
				continuation_clause->binding_id !=
					occurrence_clause->continuation_binder_id ||
				continuation_solution->usage_state !=
					OPERATION_CONSTRAINT_STATE_SOLVED ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE &&
					continuation_solution->binder_usage !=
						PROTOTYPE_USAGE_ZERO) ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
					continuation_solution->binder_usage ==
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
		uint32_t operation_classifier = operation_solver_classifier(
			ctx, occurrence_clause->operation_operation
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
		if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
			if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
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
		struct prototype_term_classifier_view operation_result_view;
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
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_result_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
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
		/* A clause argument has the operation domain even when the source body
	 * discards it. VAR occurrences are usage evidence, not typing authority. */
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->operation_count;
		++lambda_operation_id) {
		struct prototype_operation_node* lambda_operation =
			&ctx->metadata->operations[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			lambda_operation->core_term != outer_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id != occurrence_clause->argument_ast_binder_id) {
			continue;
		}
		if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
			!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				operation_domain
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			lambda_operation->binder_classifier = operation_domain;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
				return -1;
			}
		}
	}
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->operation_count;
		++lambda_operation_id) {
		struct prototype_operation_node* lambda_operation =
			&ctx->metadata->operations[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			lambda_operation->core_term != inner_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id !=
				occurrence_clause->continuation_ast_binder_id) {
			continue;
		}
			if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (operation_solver_continuation_effect_widening(
					ctx,
					lambda_operation->binder_classifier,
					continuation_binder_classifier
				)) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				continuation_binder_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		uint32_t lambda_term;
		if (operation->tag != PROTOTYPE_OPERATION_VAR) {
			continue;
		}
		if (operation->referenced_ast_binder_id ==
			occurrence_clause->argument_ast_binder_id) {
			lambda_term = outer_lambda;
		} else if (operation->referenced_ast_binder_id ==
			occurrence_clause->continuation_ast_binder_id) {
			lambda_term = inner_lambda;
		} else {
			continue;
		}
		memset(visited, 0, sizeof(visited));
		if (operation_subtree_contains_operation(
				ctx,
				occurrence_clause->body_operation,
				operation_id,
				visited
			) <= 0) {
			continue;
		}
		uint32_t classifier =
			operation->referenced_ast_binder_id ==
				occurrence_clause->argument_ast_binder_id ?
				operation_domain : PROTOTYPE_INVALID_ID;
		for (uint32_t lambda_operation_id = 0;
			classifier == PROTOTYPE_INVALID_ID &&
				lambda_operation_id < ctx->metadata->operation_count;
			++lambda_operation_id) {
			const struct prototype_operation_node* lambda_operation =
				&ctx->metadata->operations[lambda_operation_id];
			uint32_t ast_binder_id;
			if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
				lambda_operation->core_term != lambda_term ||
				operation_solver_lambda_ast_binder(
					ctx, lambda_operation_id, &ast_binder_id
				) != 0 ||
				ast_binder_id != operation->referenced_ast_binder_id) {
				continue;
			}
			if (lambda_operation->binder_classifier != PROTOTYPE_INVALID_ID) {
				classifier = lambda_operation->binder_classifier;
				break;
			}
			if (lambda_operation->body >= ctx->metadata->operation_count) {
				continue;
			}
			const struct prototype_operation_node* lambda_body_operation =
				&ctx->metadata->operations[lambda_operation->body];
			const struct prototype_context* lambda_binder_context =
				prototype_context_get(
					&ctx->metadata->contexts, lambda_body_operation->context_id
				);
			uint32_t binder_var;
			if (!lambda_binder_context ||
				lambda_binder_context->parent != lambda_operation->context_id ||
				prototype_term_var(
					ctx->terms, lambda_binder_context->binding_id, &binder_var
				) != 0) {
				continue;
			}
			for (size_t relation_id = 0;
				relation_id < ctx->judgement_delta.proposition_count;
				++relation_id) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[relation_id];
				uint32_t assumption_context_id;
				if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					relation->subject != binder_var ||
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) != 0 ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						relation->context_id,
						ctx->terms->terms[relation->subject].as.var.binding_id,
						&assumption_context_id
					) != 0) {
					continue;
				}
				const struct prototype_context* assumption_context =
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context_id
					);
				const struct prototype_context* argument_context = assumption_context ?
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context->parent
					) : NULL;
				uint32_t argument_classifier =
					prototype_context_classifier_term(argument_context);
				if (!argument_context ||
					argument_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						argument_classifier,
						operation_domain
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				classifier = relation->classifier;
				break;
			}
			break;
		}
			if (classifier != PROTOTYPE_INVALID_ID &&
				(operation->referenced_ast_binder_id ==
					occurrence_clause->continuation_ast_binder_id ?
					operation_solver_widen_continuation_binding(
						ctx, operation_id, classifier, p_changed
					) : operation_solver_bind_proven_classifier(
						ctx, operation_id, classifier, p_changed
					)) != 0) {
				return -1;
			}
			if (classifier != PROTOTYPE_INVALID_ID) {
				for (uint32_t lambda_operation_id = 0;
					lambda_operation_id < ctx->metadata->operation_count;
					++lambda_operation_id) {
					struct prototype_operation_node* lambda_operation =
						&ctx->metadata->operations[lambda_operation_id];
					uint32_t ast_binder_id;
					if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
						lambda_operation->core_term != lambda_term ||
						operation_solver_lambda_ast_binder(
							ctx, lambda_operation_id, &ast_binder_id
						) != 0 ||
						ast_binder_id != operation->referenced_ast_binder_id) {
						continue;
					}
					if (lambda_operation->binder_classifier ==
							PROTOTYPE_INVALID_ID ||
						(operation->referenced_ast_binder_id ==
							occurrence_clause->argument_ast_binder_id &&
						 !(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						 ).status == PROTOTYPE_TERM_CONVERSION_EQUAL))) {
						lambda_operation->binder_classifier = classifier;
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (operation->referenced_ast_binder_id ==
							occurrence_clause->continuation_ast_binder_id &&
						operation_solver_continuation_effect_widening(
							ctx,
							lambda_operation->binder_classifier,
							classifier
						)) {
						lambda_operation->binder_classifier = classifier;
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (
						!(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int operation_solver_solve(struct compile_context* ctx, int require_complete) {
	if (!ctx || operation_solver_generate_constraints(ctx) != 0) {
		return -1;
	}
	int changed = 0;
	if (operation_solver_seed_known_classifiers(ctx, &changed) != 0) {
		return -1;
	}
	for (;;) {
		uint32_t i;
		int pop_status;
		while ((pop_status = operation_solver_pop_constraint(ctx, &i)) == 0) {
			int pass_changed = 0;
			const struct operation_classifier_constraint* constraint =
				&ctx->classifier_solver.constraints[i];
			struct operation_classifier_constraint_result* constraint_result =
				&ctx->classifier_solver.constraint_results[i];
			uint32_t target = constraint->classifier_variable;
			if (ctx->metadata->solver_steps_used >= ctx->metadata->solver_step_limit) {
				ctx->metadata->solver_exhausted = 1;
				operation_solver_refresh_constraint_states(ctx, 1);
				return 1;
			}
			ctx->metadata->solver_steps_used++;
			uint32_t classifier;
			switch (constraint->kind) {
				case OPERATION_CONSTRAINT_EQUAL:
					if (constraint->payload.reference.referenced_operation >=
							ctx->metadata->operation_count ||
						(classifier = operation_solver_classifier(
							ctx, constraint->payload.reference.referenced_operation
						)) ==
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if (operation_solver_bind(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CONVERTIBLE: {
						uint32_t expected_classifier =
							constraint->payload.conversion.expected_classifier;
						if (expected_classifier == PROTOTYPE_INVALID_ID) {
							expected_classifier =
								ctx->metadata->operations[target].known_classifier;
						}
						if (constraint->payload.conversion.body_operation >=
								ctx->metadata->operation_count ||
							expected_classifier >=
								ctx->terms->term_count) {
							break;
						}
							classifier = operation_solver_classifier(
							ctx, constraint->payload.conversion.body_operation
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						/*
						 * An omitted surface effect row is an elaboration variable.
						 * Resolve it while the OperationGraph fixed point is still
						 * propagating, so downstream APP/COMPUTATION_FOLD proofs never capture the
						 * unsolved annotation. Imported transparent definitions are
						 * checked again by the dedicated ascription phase.
						 */
						uint32_t solved_expected;
						int solve_status =
							prototype_judgement_solve_expected_effect_rows(
								ctx->terms,
								ctx->type_declarations,
								NULL,
								expected_classifier,
								classifier,
								&solved_expected
							);
						if (solve_status < 0) {
							return -1;
						}
						if (solve_status != 0) {
							/* The body classifier is already a valid synthesis frontier.
							 * Preserve it for downstream operations while the dedicated
							 * ascription phase retains responsibility for proving conversion
							 * to the surface expectation. */
							if (operation_solver_bind(
									ctx,
									target,
									classifier,
									&pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						constraint_result->conversion_goal.id = constraint->id;
						constraint_result->conversion_goal.context_id =
							constraint->context_id;
						constraint_result->conversion_goal.carrier_classifier =
							PROTOTYPE_INVALID_ID;
						constraint_result->conversion_goal.left_term =
							solved_expected;
						constraint_result->conversion_goal.right_term = classifier;
						constraint_result->conversion_goal.normalization_profile =
							PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
						constraint_result->conversion_goal.step_limit =
							ctx->metadata->normalization_step_limit;
						if (prototype_judgement_kernel_conversion_goal_execute(
								&ctx->metadata->contexts,
								ctx->terms,
								ctx->type_declarations,
								NULL,
								&constraint_result->conversion_goal,
								0
							) != 0) {
							return -1;
						}
						if (constraint_result->conversion_goal.result.status !=
							PROTOTYPE_TERM_CONVERSION_EQUAL) {
							if (constraint_result->conversion_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_INVALID) {
								return -1;
							}
							if (constraint_result->conversion_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
								constraint_result->state =
									OPERATION_CONSTRAINT_STATE_CONTRADICTION;
								constraint_result->reason =
									OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED;
							}
							if (operation_solver_bind(
									ctx, target, classifier, &pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						ctx->metadata->operations[target].known_classifier =
							solved_expected;
						if (operation_solver_bind(
								ctx,
								target,
								solved_expected,
								&pass_changed
							) != 0) {
							return -1;
							}
							break;
						}
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (constraint->payload.pi.body_or_function_operation >=
							ctx->metadata->operation_count ||
						(constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
							constraint->payload.pi.domain_classifier_or_argument_operation >=
								ctx->metadata->operation_count)) {
						break;
					}
					if (constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
						uint32_t expected =
							ctx->classifier_solver.solutions[target].classifier;
						uint32_t current_body_classifier = operation_solver_classifier(
							ctx, constraint->payload.pi.body_or_function_operation
						);
						if (expected != PROTOTYPE_INVALID_ID &&
							current_body_classifier == PROTOTYPE_INVALID_ID) {
							uint32_t domain;
							uint32_t codomain_family;
							uint32_t codomain_binder;
							uint32_t codomain_body;
							uint32_t binder_var;
							uint32_t body_classifier;
							const struct prototype_operation_node* lambda =
								&ctx->metadata->operations[target];
							if (lambda->core_term >= ctx->terms->term_count ||
								ctx->terms->terms[lambda->core_term].tag !=
									PROTOTYPE_TERM_LAMBDA ||
								prototype_judgement_pi_parts(
									ctx->terms, expected, &domain, &codomain_family
								) != 0 ||
								prototype_term_pure_family_parts(
									ctx->terms,
									codomain_family,
									&codomain_binder,
									&codomain_body
								) != 0 ||
								!(prototype_judgement_classifier_conversion(
									ctx->terms, ctx->type_declarations,
									domain,
									lambda->binder_classifier
								).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
								prototype_term_var(
									ctx->terms,
									ctx->terms->terms[lambda->core_term].as.lambda.binding_id,
									&binder_var
								) != 0 ||
								prototype_term_graph_substitute_bound_var(
									ctx->terms,
									ctx->type_declarations,
									codomain_body,
									codomain_binder,
									binder_var,
									&body_classifier
								) != 0 ||
								operation_solver_bind(
									ctx,
									constraint->payload.pi.body_or_function_operation,
									body_classifier,
									&pass_changed
								) != 0) {
								/* The expected Pi may be more reduced than the current
								 * operation frontier. Leave this reverse propagation for a
								 * later pass; the forward Pi constraint remains authoritative. */
								break;
							}
						}
						classifier = operation_solver_classifier(
							ctx, constraint->payload.pi.body_or_function_operation
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t codomain_family;
						const struct prototype_operation_node* lambda =
							&ctx->metadata->operations[target];
						uint32_t effective_domain = lambda->binder_classifier;
						if (effective_domain == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (lambda->core_term >= ctx->terms->term_count ||
							ctx->terms->terms[lambda->core_term].tag !=
								PROTOTYPE_TERM_LAMBDA ||
							prototype_term_pure_family(
								ctx->terms,
								ctx->terms->terms[lambda->core_term].as.lambda.binding_id,
								classifier,
								&codomain_family
							) != 0 ||
							prototype_term_pi_family(
								ctx->terms,
								effective_domain,
								codomain_family,
								&classifier
							) != 0 || operation_solver_generalize_lambda_effect_rows(
								ctx, target, classifier, &classifier
							) != 0 || operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					} else {
						uint32_t function_classifier =
							operation_solver_classifier(
								ctx, constraint->payload.pi.body_or_function_operation
							);
						uint32_t argument_classifier =
							operation_solver_classifier(
								ctx,
								constraint->payload.pi.domain_classifier_or_argument_operation
							);
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (operation_solver_specialize_integer_literal(
								ctx,
								function_classifier,
								constraint->payload.pi.domain_classifier_or_argument_operation,
								&argument_classifier,
								&pass_changed
							) != 0) {
							return -1;
						}
						int apply_status = operation_apply_classifier(
							ctx,
							function_classifier,
							argument_classifier,
							ctx->metadata->operations[
								constraint->payload.pi.domain_classifier_or_argument_operation
							].core_term,
							&classifier
						);
						/* A non-Pi or incompatible current candidate may belong to a
						 * shared core node. Leave this operation constraint unresolved
						 * until a source-operation binding selects its classifier. */
						if (apply_status == 0 && operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION: {
					int constructor_status =
						operation_solver_constructor_spine_classifier(
							ctx, target, &classifier
						);
					if (constructor_status < 0) {
						return -1;
					}
					if (constructor_status == 0 && operation_solver_bind(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_HAS_TYPE:
					if (ctx->classifier_solver.solutions[target].classifier !=
						PROTOTYPE_INVALID_ID) {
						break;
					}
					/* Core facts may seed atoms only, never a typed operation edge. */
					if (ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_APP ||
						ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_LAMBDA ||
						ctx->metadata->operations[target].tag ==
							PROTOTYPE_OPERATION_MATCH) {
						break;
					}
					{
						uint32_t classifiers[32];
						uint32_t classifier_count = 0;
						if (operation_solver_collect_input_classifiers(
								ctx,
								target,
								classifiers,
								32,
								&classifier_count
							) != 0) {
							return -1;
						}
						if (classifier_count == 1 && operation_solver_bind(
								ctx, target, classifiers[0], &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
					if (operation_solver_solve_match(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_IH_EXPECTED:
					if (operation_solver_materialize_induction_hypothesis(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
					if (constraint->payload.cbpv_boundary.child_operation >=
						ctx->metadata->operation_count) {
						return -1;
					}
					uint32_t child_classifier = operation_solver_classifier(
						ctx, constraint->payload.cbpv_boundary.child_operation
					);
					if (child_classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					if (prototype_judgement_cbpv_boundary_classifier(
							ctx->terms,
							ctx->type_declarations,
							ctx->metadata->operations[target].core_term,
							child_classifier,
							&classifier
						) != 0 || operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT: {
					const struct prototype_operation_node* fold =
						&ctx->metadata->operations[target];
					if (fold->core_term >= ctx->terms->term_count ||
						ctx->terms->terms[fold->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
						return -1;
					}
					int status = ctx->terms->terms[fold->core_term].as.computation_fold.clause_count == 0 ?
						operation_solver_propagate_zero_clause_computation_fold_input(
							ctx, target, &pass_changed
						) : operation_solver_propagate_clause_computation_fold_input(
							ctx, target, &pass_changed
						);
					if (status != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
					/* These occurrence constraints are solved by the CBPV computation
					 * constraint pass. Their state is published only after that pass has
					 * either produced closed evidence or a VerificationDB obligation. */
					break;
				default:
					return -1;
			}
			if (pass_changed) {
				changed = 1;
			}
		}
		if (pop_status < 0) {
			return -1;
		}
		int materialized = 0;
		if (operation_solver_materialize_solved_motives(ctx, &materialized) != 0) {
			return -1;
		}
		if (!materialized) {
			if (require_complete && operation_solver_require_complete(ctx) != 0) {
				operation_solver_refresh_constraint_states(ctx, 1);
				return -1;
			}
			int commit_status = operation_solver_commit_bindings(ctx, &changed);
			operation_solver_refresh_constraint_states(ctx, commit_status != 0);
			return commit_status;
		}
		changed = 1;
	}
}

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
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		if (ctx->metadata->operations[i].classifier != PROTOTYPE_INVALID_ID) {
			count++;
		}
	}
	return count;
}

static int build_operation_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[match->as.match.first_case + case_index];
		if (!equation || equation->payload.motive_case.branch_operation >= ctx->metadata->operation_count ||
			source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256) {
			return 1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64 ||
			prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->operation_cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0 ||
			reindexed_binder_count != source_case->binder_count) {
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
		binder_cursor += source_case->binder_count;
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t binder_var;
	uint32_t motive_match;
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_match(ctx->terms, binder_var, motive_cases, operation->case_count, &motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, binding_id, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

static int operation_usage_solution_view(
	const struct compile_context* ctx,
	uint32_t operation_id,
	struct prototype_usage_vector* p_vector
) {
	if (!ctx || !ctx->metadata || !p_vector ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	return prototype_operation_usage_solution_view(
		ctx->operation_usage_solutions,
		ctx->metadata->operation_count,
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
		ctx->metadata->operation_count > 4096) {
		return -1;
	}
	struct prototype_operation_graph operations = {
		.operations = ctx->metadata->operations,
		.operation_count = ctx->metadata->operation_count,
		.operation_capacity = ctx->metadata->operation_capacity,
		.cases = ctx->metadata->operation_cases,
		.case_count = ctx->metadata->operation_case_count,
		.case_capacity = ctx->metadata->operation_case_capacity,
		.fold_clauses = ctx->metadata->operation_fold_clauses,
		.fold_clause_count = ctx->metadata->operation_fold_clause_count,
		.fold_clause_capacity = ctx->metadata->operation_fold_clause_capacity
	};
	size_t entry_count;
	if (prototype_operation_usage_solve(
			&operations,
			ctx->terms,
			ctx->operation_usage_solutions,
			PROTOTYPE_OPERATION_USAGE_SOLUTION_CAPACITY,
			ctx->operation_usage_entries,
			ctx->operation_usage_entry_capacity,
			&entry_count
		) != 0 || entry_count > UINT32_MAX) {
		return -1;
	}
	ctx->operation_usage_entry_count = (uint32_t)entry_count;
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_usage_solution* source =
			&ctx->operation_usage_solutions[operation_id];
		struct operation_solver_solution* target =
			&ctx->classifier_solver.solutions[operation_id];
		target->first_usage_entry = source->first_entry;
		target->usage_entry_count = source->entry_count;
		target->binder_usage = source->binder_usage;
		target->usage_state = OPERATION_CONSTRAINT_STATE_SOLVED;
	}
	return 0;
}

static int operation_subtree_references_ast_binder(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t ast_binder_id,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	if (visited[operation_id]) {
		return 0;
	}
	visited[operation_id] = 1;
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_VAR) {
		return operation->referenced_ast_binder_id == ast_binder_id;
	}
	uint32_t children[66];
	uint32_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			if (operation->fold_clause_count == 0) {
				children[child_count++] = operation->argument;
				break;
			}
			children[child_count++] = operation->scrutinee;
			if (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				operation->fold_clause_count >
					ctx->metadata->operation_fold_clause_count -
						operation->first_fold_clause ||
				child_count + 2 * operation->fold_clause_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				children[child_count++] = clause->operation_operation;
				children[child_count++] = clause->body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			if (operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count || child_count + operation->case_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				children[child_count++] = ctx->metadata->operation_cases[
					operation->first_case + i
				].body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int status = operation_subtree_references_ast_binder(
			ctx, children[i], ast_binder_id, visited
		);
		if (status != 0) {
			return status;
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
		root_operation >= ctx->metadata->operation_count ||
		target_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[root_operation];
	uint32_t children[66];
	uint32_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			if (operation->fold_clause_count == 0) {
				children[child_count++] = operation->argument;
				break;
			}
			children[child_count++] = operation->scrutinee;
			if (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				operation->fold_clause_count >
					ctx->metadata->operation_fold_clause_count -
						operation->first_fold_clause ||
				child_count + 2 * operation->fold_clause_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				children[child_count++] = clause->operation_operation;
				children[child_count++] = clause->body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			if (operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count || child_count + operation->case_count > 66) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				children[child_count++] = ctx->metadata->operation_cases[
					operation->first_case + i
				].body_operation;
			}
			break;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int status = operation_subtree_contains_operation(
			ctx, children[i], target_operation, visited
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
		root_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t target = 0;
		target < ctx->metadata->operation_count;
		++target) {
		uint32_t core_term = ctx->metadata->operations[target].core_term;
		if (core_term >= ctx->terms->term_count ||
			ctx->terms->terms[core_term].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		uint8_t visited[ctx->metadata->operation_count];
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

static int operation_branch_uses_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_constraint* equation
) {
	if (!ctx || !equation ||
		equation->classifier_variable >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[equation->classifier_variable];
	if (match->first_case + equation->payload.motive_case.case_index >=
		ctx->metadata->operation_case_count) {
		return -1;
	}
	const struct prototype_operation_match_case* operation_case =
		&ctx->metadata->operation_cases[
			match->first_case + equation->payload.motive_case.case_index
		];
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		uint8_t visited[4096] = { 0 };
		int status = operation_subtree_references_ast_binder(
			ctx, equation->payload.motive_case.branch_operation,
			operation_case->ast_binder_ids[i],
			visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_constraint* equation,
	uint32_t classifier
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		equation->classifier_variable >= ctx->metadata->operation_count) {
		return -1;
	}
	int source_uses_binder = operation_branch_uses_case_binder(ctx, equation);
	if (source_uses_binder <= 0) {
		return source_uses_binder;
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
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[equation->classifier_variable];
	uint32_t context_path[64];
	uint32_t context_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			match->context_id,
			ctx->metadata->operation_cases[
				match->first_case + equation->payload.motive_case.case_index
			].context_id,
			context_path,
			64,
			&context_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < context_count; ++i) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_path[i]
		);
		if (!context) {
			return -1;
		}
		uint32_t binding_id = context->binding_id;
		if (prototype_term_contains_free_binding(
			ctx->terms, classifier_whnf, binding_id
		) != 0) {
			return 1;
		}
	}
	return 0;
}

/*
 * A uniform motive is represented as a constant lambda. This is not a Match
 * typing shortcut: APP(\_ => T, scrutinee) reduces by beta to T. It is valid
 * only when no branch classifier mentions that branch's pattern binders.
 */
static int build_operation_uniform_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		if (!equation || equation->payload.motive_case.branch_operation >=
			ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		int capture_status = operation_classifier_captures_case_binder(
			ctx, equation, branch_classifier
		);
		if (capture_status < 0) {
			return -1;
		}
		if (capture_status > 0) {
			return 1;
		}
		if (classifier == PROTOTYPE_INVALID_ID) {
			classifier = branch_classifier;
		} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, classifier, branch_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
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
static int build_operation_guarded_recursive_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->operation_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
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
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, typing->operation, case_index
			);
		uint32_t source_case_id = match->as.match.first_case + case_index;
		if (!equation || source_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[source_case_id];
		if (source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64 ||
			prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->operation_cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0 ||
			reindexed_binder_count != source_case->binder_count) {
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
			ctx->classifier_solver.solutions[
				equation->payload.motive_case.branch_operation
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
			const struct prototype_operation_node* branch =
				&ctx->metadata->operations[
					equation->payload.motive_case.branch_operation
				];
			if (branch->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
				branch->argument >= ctx->metadata->operation_count ||
				branch->scrutinee != typing->operation) {
				return 1;
			}
			uint32_t original_argument =
				ctx->metadata->operations[branch->argument].core_term;
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
					source_binder->binding_id ==
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
		prototype_term_set_ih_scope_term(ctx->terms, motive_frame, motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, motive_binder, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

/*
 * This is the solver occurs check for the only recursive metavariable form
 * currently admitted by the language: an IH denotes M(rest). The occurrence
 * must point back to its own Match frame and `rest` must be a recursive field
 * of that Match. A raw M(x) self-reference outside that structural edge is
 * rejected instead of becoming a cyclic solver substitution.
 */
static int operation_solver_validate_guarded_motive_occurrence(
	const struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_operation
) {
	if (!ctx || !ctx->metadata ||
		ih_operation >= ctx->metadata->operation_count ||
		motive_operation >= ctx->metadata->operation_count ||
		argument_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* ih =
		&ctx->metadata->operations[ih_operation];
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[motive_operation];
	const struct prototype_operation_node* argument =
		&ctx->metadata->operations[argument_operation];
	if (ih->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		match->tag != PROTOTYPE_OPERATION_MATCH ||
		ih->first_case >= ctx->terms->ih_scope_count ||
		argument->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[argument->core_term].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	if (ctx->terms->ih_scopes[ih->first_case].match_term != match->core_term ||
		match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t binding_id = ctx->terms->terms[argument->core_term].as.var.binding_id;
	const struct prototype_term* match_term = &ctx->terms->terms[match->core_term];
	for (uint32_t case_index = 0; case_index < match_term->as.match.case_count;
		++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match_term->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* binder =
				&ctx->terms->case_binders[match_case->first_binder + binder_index];
			if (binder->binding_id == binding_id) {
				return binder->is_recursive ? 0 : -1;
			}
		}
	}
	return -1;
}

static int operation_solver_nonrecursive_seed_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	const struct prototype_operation_node* operation,
	uint32_t* p_classifier,
	uint32_t* p_source_body_operation
) {
	if (!ctx || !operation || !p_classifier || !p_source_body_operation ||
		operation_id >= ctx->metadata->operation_count || operation->case_count == 0 ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return -1;
	}
	uint32_t candidate = PROTOTYPE_INVALID_ID;
	uint32_t source_body_operation = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation || equation->payload.motive_case.branch_operation >=
			ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.motive_case.branch_operation
			].classifier;
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
		if (has_recursive_binder) {
			continue;
		}
		if (candidate == PROTOTYPE_INVALID_ID) {
			candidate = branch_classifier;
			source_body_operation = equation->payload.motive_case.branch_operation;
		} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, candidate, branch_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
	}
	if (candidate == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = candidate;
	*p_source_body_operation = source_body_operation;
	return 0;
}

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
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
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	int has_unresolved_ih = 0;
	const struct prototype_operation_node* match =
		&ctx->metadata->operations[operation_id];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		if (ctx->classifier_solver.solutions[
				equation->payload.motive_case.branch_operation
			].classifier !=
			PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_operation_node* body =
			&ctx->metadata->operations[
				equation->payload.motive_case.branch_operation
			];
		if (body->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			body->scrutinee != operation_id) {
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
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	if (operation->tag != PROTOTYPE_OPERATION_MATCH || !typing ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return -1;
	}
	if (ctx->classifier_solver.solutions[operation_id].classifier !=
			PROTOTYPE_INVALID_ID ||
		ctx->classifier_solver.motive_solution_states[operation_id] ==
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int has_unresolved_body = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		uint32_t body_operation = equation->payload.motive_case.branch_operation;
		if (body_operation >= ctx->metadata->operation_count ||
			ctx->classifier_solver.solutions[body_operation].classifier ==
				PROTOTYPE_INVALID_ID) {
			has_unresolved_body = 1;
		}
	}
	if (!has_unresolved_body) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_READY;
		return 0;
	}
	int has_recursive_binder = operation_solver_match_has_recursive_binder(ctx, operation);
	if (has_recursive_binder < 0) {
		return -1;
	}
	if (!has_recursive_binder) {
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
	uint32_t seed_source_body_operation;
	int seed_status = operation_solver_nonrecursive_seed_classifier(
		ctx, operation_id, operation, &seed_classifier, &seed_source_body_operation
	);
	if (seed_status < 0) {
		return -1;
	}
	if (seed_status == 0) {
		seed_status = operation_solver_seed_motive(
			ctx, operation_id, seed_classifier, seed_source_body_operation, p_changed
		);
		return seed_status < 0 ? -1 : 0;
	}
	int guarded_status = operation_solver_has_guarded_recursive_equation(ctx, operation_id);
	if (guarded_status < 0) {
		return -1;
	}
	if (guarded_status > 0) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE;
		return 0;
	}
	return 0;
}

static int operation_solver_materialize_match_solution(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	uint8_t state = ctx->classifier_solver.motive_solution_states[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH || !typing ||
		state == OPERATION_MOTIVE_SOLUTION_UNRESOLVED ||
		state == OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	uint32_t classifier;
	int status;
	if (state == OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE) {
		status = build_operation_guarded_recursive_motive(ctx, typing, &classifier);
	} else {
		status = build_operation_uniform_motive(ctx, typing, &classifier);
		if (status > 0) {
			status = build_operation_motive(ctx, typing, &classifier);
		}
	}
	if (status != 0) {
		return status < 0 ? -1 : 0;
	}
	uint32_t seed_classifier =
		ctx->classifier_solver.motive_constant_candidates[operation_id];
	if (seed_classifier != PROTOTYPE_INVALID_ID &&
		!(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, classifier, seed_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	ctx->classifier_solver.motive_terms[operation_id] =
		ctx->terms->terms[classifier].as.app.function;
	ctx->classifier_solver.motive_solution_states[operation_id] =
		OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
	return operation_solver_bind(ctx, operation_id, classifier, p_changed);
}

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		if (ctx->metadata->operations[i].tag == PROTOTYPE_OPERATION_MATCH &&
			operation_solver_materialize_match_solution(ctx, i, p_changed) != 0) {
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
	if (!ctx || !p_changed || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	if (ctx->classifier_solver.solutions[operation_id].classifier !=
		PROTOTYPE_INVALID_ID) {
		return 0;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		operation->argument >= ctx->metadata->operation_count ||
		operation->scrutinee >= ctx->metadata->operation_count ||
		ctx->metadata->operations[operation->scrutinee].tag !=
			PROTOTYPE_OPERATION_MATCH) {
		return -1;
	}
	uint32_t parent_match = operation->scrutinee;
	if (operation_solver_solve_match(ctx, parent_match, p_changed) != 0) {
		return -1;
	}
	uint32_t motive = ctx->classifier_solver.motive_terms[parent_match];
	if (motive == PROTOTYPE_INVALID_ID) {
		uint32_t parent_classifier = operation_solver_classifier(ctx, parent_match);
		const struct prototype_operation_node* match_operation =
			&ctx->metadata->operations[parent_match];
		if (parent_classifier != PROTOTYPE_INVALID_ID &&
			parent_classifier < ctx->terms->term_count &&
			ctx->terms->terms[parent_classifier].tag == PROTOTYPE_TERM_APP &&
			match_operation->scrutinee < ctx->metadata->operation_count &&
			ctx->terms->terms[parent_classifier].as.app.argument ==
				ctx->metadata->operations[match_operation->scrutinee].core_term) {
			motive = ctx->terms->terms[parent_classifier].as.app.function;
			ctx->classifier_solver.motive_terms[parent_match] = motive;
			ctx->classifier_solver.motive_solution_states[parent_match] =
				OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		}
	}
	if (motive == PROTOTYPE_INVALID_ID) {
		if (operation_solver_validate_guarded_motive_occurrence(
				ctx, operation_id, parent_match, operation->argument
			) != 0) {
			return -1;
		}
		if (operation_solver_set_ih_motive_application(
				ctx, operation_id, parent_match, operation->argument, p_changed
			) != 0) {
			return -1;
		}
		/* A constant motive equation M(_) == T is already a concrete solver
		 * solution for this IH occurrence.  Bind it now so CBPV wrappers can
		 * propagate through RETURN/THUNK/COMPUTATION_FOLD; the lambda for M is still
		 * materialized only after every Match equation has been checked. */
		uint32_t candidate =
			ctx->classifier_solver.motive_constant_candidates[parent_match];
		if (candidate != PROTOTYPE_INVALID_ID) {
			return operation_solver_bind(ctx, operation_id, candidate, p_changed);
		}
		return 0;
	}
	uint32_t classifier;
	if (prototype_term_app(
			ctx->terms, motive,
			ctx->metadata->operations[operation->argument].core_term,
			&classifier
		) != 0) {
		return -1;
	}
	return operation_solver_bind(ctx, operation_id, classifier, p_changed);
}

static int operation_solver_materialize_induction_hypothesis_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	uint32_t classifier = operation->classifier;
	if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		classifier == PROTOTYPE_INVALID_ID ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		return -1;
	}
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* relations =
			source == 0 ? ctx->judgement_delta.propositions : ctx->judgement->propositions;
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? ctx->judgement_delta.derivation_candidates :
				ctx->judgement->derivation_candidates;
		size_t proposition_count =
			source == 0 ? ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
		size_t derivation_candidate_count = source == 0 ?
			ctx->judgement_delta.derivation_candidate_count :
			ctx->judgement->derivation_candidate_count;
		for (size_t i = 0; i < proposition_count; ++i) {
			if (relations[i].kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relations[i].operation_id == operation_id &&
				relations[i].subject == operation->core_term &&
				prototype_judgement_candidate_find_derivation_kind(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
					&(uint32_t){0}
				) == 0 &&
				(prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					relations[i].classifier, classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return 0;
			}
		}
	}
	const struct prototype_term* ih = &ctx->terms->terms[operation->core_term];
	uint32_t ih_scope_id = ih->as.induction_hypothesis.ih_scope_id;
	if (ih_scope_id >= ctx->terms->ih_scope_count ||
		ih->as.induction_hypothesis.argument >= ctx->terms->term_count ||
		ctx->terms->terms[ih->as.induction_hypothesis.argument].tag !=
			PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t match_term = ctx->terms->ih_scopes[ih_scope_id].match_term;
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		operation->scrutinee >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t match_classifier = ctx->metadata->operations[
		operation->scrutinee
	].classifier;
	if (match_classifier >= ctx->terms->term_count ||
		ctx->terms->terms[match_classifier].tag != PROTOTYPE_TERM_APP) {
		return -1;
	}
	uint32_t motive = ctx->terms->terms[match_classifier].as.app.function;
	const struct prototype_term* match = &ctx->terms->terms[match_term];
	uint32_t binding_id =
		ctx->terms->terms[ih->as.induction_hypothesis.argument].as.var.binding_id;
	for (uint32_t case_index = 0; case_index < match->as.match.case_count;
		++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* binder = &ctx->terms->case_binders[
				match_case->first_binder + binder_index
			];
			if (binder->binding_id != binding_id) {
				continue;
			}
			if (!binder->is_recursive) {
				return -1;
			}
			return prototype_judgement_delta_expand_induction_hypothesis(
				&ctx->judgement_delta, ctx->terms, operation->core_term, classifier,
				match_term, motive, case_index, binder_index
			);
		}
	}
	return -1;
}

/*
 * The solver owns classifier synthesis. Once all operation variables have a
 * concrete solution, this phase only reconstructs JudgementDB derivations for
 * those already-fixed conclusions. It is deliberately not a second inference
 * pass: every generated conclusion must be normalization-equal to the solver
 * binding for the same operation.
 */
static int operation_match_cases_are_resolved(
	const struct compile_context* ctx,
	const struct prototype_operation_node* operation
) {
	if (!ctx || !ctx->metadata || !operation ||
		operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->first_case + operation->case_count >
			ctx->metadata->operation_case_count) {
		return 0;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct prototype_operation_match_case* operation_case =
			&ctx->metadata->operation_cases[operation->first_case + case_index];
		if (operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
	}
	return 1;
}

static int operation_solver_materialize_match_pattern_assumptions(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
			operation->source_ast >= ctx->asts->node_count ||
			ctx->asts->nodes[operation->source_ast].tag != PROTOTYPE_AST_MATCH) {
			continue;
		}
		/* Constructor selection depends on the scrutinee classifier. The solver
		 * may have learned that classifier only in this fixed-point round, so
		 * defer proof materialization until the following round resolves cases. */
		if (!operation_match_cases_are_resolved(ctx, operation)) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_ast_node* source_match =
			&ctx->asts->nodes[operation->source_ast];
		if (match->as.match.case_count != operation->case_count ||
			source_match->as.match.case_count != operation->case_count ||
			operation->first_case + operation->case_count >
				ctx->metadata->operation_case_count) {
			return -1;
		}
		for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[operation->first_case + case_index];
			uint32_t term_case_id = match->as.match.first_case + case_index;
			uint32_t ast_case_id = source_match->as.match.first_case + case_index;
			if (term_case_id >= ctx->terms->case_count || ast_case_id >= ctx->asts->case_count ||
				operation_case->body_operation >= ctx->metadata->operation_count ||
				operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation_case->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, PROTOTYPE_INVALID_ID
			);
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[term_case_id];
			const struct prototype_ast_match_case* ast_case =
				&ctx->asts->cases[ast_case_id];
			if (match_case->binder_count != ast_case->binder_count ||
				match_case->first_binder + match_case->binder_count >
					ctx->terms->case_binder_count ||
				ast_case->first_binder + ast_case->binder_count >
					ctx->asts->case_binder_count) {
				return -1;
			}
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count;
				++binder_index) {
				uint32_t classifier;
				uint32_t binder_var;
				if (constructor_telescope_field_classifier(
						ctx,
						operation_case->context_id,
						operation_case->constructor_owner,
						operation_case->constructor_id,
						&ctx->terms->case_binders[match_case->first_binder],
						binder_index,
						binder_index,
						&classifier
					) != 0 ||
					prototype_term_var(
						ctx->terms,
						ctx->terms->case_binders[
							match_case->first_binder + binder_index
						].binding_id,
						&binder_var
					) != 0) {
					return -1;
				}
				int already_materialized = 0;
				for (size_t relation_id = 0;
					relation_id < ctx->judgement_delta.proposition_count;
					++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&ctx->judgement_delta.propositions[relation_id];
					if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						relation->context_id != operation_case->context_id ||
						relation->subject != binder_var ||
						relation->classifier != classifier) {
						continue;
					}
					uint32_t cursor = 0;
					uint32_t derivation_id;
					while (prototype_judgement_candidate_derivation_next(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						&cursor,
						&derivation_id
					) == 0) {
						const struct prototype_judgement_derivation_candidate* proof =
							&ctx->judgement_delta.derivation_candidates[derivation_id];
						if (proof->proof_kind ==
								PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION &&
							proof->rule_data.constructor.owner_view ==
							operation_case->constructor_owner &&
							proof->rule_data.constructor.constructor_index ==
							operation_case->constructor_id &&
							proof->rule_data.constructor.field_index == binder_index) {
							already_materialized = 1;
							break;
						}
					}
					if (already_materialized) {
						break;
					}
				}
				if (already_materialized) {
					continue;
				}
				if (prototype_judgement_delta_record_match_pattern(
						&ctx->judgement_delta,
						ctx->terms,
						binder_var,
						classifier,
						operation_case->constructor_owner,
						operation_case->constructor_id,
						binder_index
					) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

static uint32_t operation_solver_evidence_owner(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	for (uint32_t depth = 0;
		ctx && ctx->metadata && operation_id < ctx->metadata->operation_count &&
		depth < 64;
		++depth) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		int authority_neutral_atom = operation->tag == PROTOTYPE_OPERATION_ATOM &&
			operation->core_term < ctx->terms->term_count &&
			(ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EXTERNAL_REF ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_PURE_PRIMITIVE ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EFFECT_OPERATION ||
			 prototype_term_type_instance_info(
				ctx->terms,
				operation->core_term,
				&(uint32_t){0},
				NULL,
				&(uint32_t){0}
			) == 0);
		if (authority_neutral_atom ||
			operation->tag == PROTOTYPE_OPERATION_VAR ||
			operation->tag == PROTOTYPE_OPERATION_CONSTRUCTOR) {
			return PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			operation_id = operation->function;
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
			operation->body < ctx->metadata->operation_count &&
			operation->classifier ==
				ctx->metadata->operations[operation->body].classifier) {
			operation_id = operation->body;
			continue;
		}
		return operation_id;
	}
	return operation_id;
}

static int operation_solver_refresh_delta_resource_usage(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement_delta.propositions[i];
		if (proposition->operation_id != PROTOTYPE_INVALID_ID) {
			struct prototype_usage_vector usage;
			if (operation_usage_solution_view(
					ctx, proposition->operation_id, &usage
				) != 0 || usage.count > UINT32_MAX) {
				return -1;
			}
			proposition->resource_usage_count = (uint32_t)usage.count;
			proposition->resource_usage = usage.entries;
		}
	}
	return 0;
}

static int operation_solver_delta_select_evidence(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!ctx || operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	return prototype_judgement_delta_select_evidence(
		&ctx->judgement_delta,
		operation_id,
		context_id,
		subject,
		classifier,
		selected
	);
}

static int operation_solver_select_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	uint32_t source_context = context_id;
	for (;;) {
		struct prototype_judgement_selected_evidence source_evidence;
		int selection_status = operation_solver_delta_select_evidence(
			ctx,
			expected_operation_id,
			source_context,
			subject,
			classifier,
			&source_evidence
		);
		if (selection_status < 0 || selection_status == 2) {
			return -1;
		}
		if (selection_status == 0) {
			if (source_context == context_id) {
				if (p_evidence) {
					*p_evidence = source_evidence;
				}
				return 0;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, expected_operation_id
			);
			uint32_t weakening_substitution;
			if (prototype_substitution_projection_path(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					context_id,
					source_context,
					&weakening_substitution
				) != 0) {
				return -1;
			}
			int weaken_status = prototype_judgement_delta_record_context_weaken(
				&ctx->judgement_delta,
				&source_evidence,
				weakening_substitution
			);
			if (weaken_status != 0 || !p_evidence) {
				return weaken_status;
			}
			return operation_solver_delta_select_evidence(
				ctx,
				expected_operation_id,
				context_id,
				subject,
				classifier,
				p_evidence
			);
		}
		if (source_context == 0) {
			break;
		}
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, source_context
		);
		if (!context || context->parent >= source_context) {
			return -1;
		}
		source_context = context->parent;
	}
	return 1;
}

static int operation_solver_require_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	return operation_solver_select_evidence_in_context(
		ctx,
		expected_operation_id,
		context_id,
		subject,
		classifier,
		NULL
	);
}

static int operation_solver_lookup_operation_derivation_classifier(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t subject,
	int proof_kind,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (size_t i = ctx->judgement_delta.proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation =
			&ctx->judgement_delta.propositions[i - 1];
		if (relation->operation_id == operation_id &&
			relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			prototype_judgement_candidate_find_derivation_kind(
				ctx->judgement_delta.propositions,
				ctx->judgement_delta.proposition_count,
				ctx->judgement_delta.derivation_candidates,
				ctx->judgement_delta.derivation_candidate_count,
				(uint32_t)(i - 1),
				proof_kind,
				&(uint32_t){0}
			) == 0) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	if (ctx->judgement_delta.db) {
		for (size_t i = ctx->judgement_delta.db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation =
				&ctx->judgement_delta.db->propositions[i - 1];
			if (relation->operation_id == operation_id &&
				relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				prototype_judgement_candidate_find_derivation_kind(
					ctx->judgement_delta.db->propositions,
					ctx->judgement_delta.db->proposition_count,
					ctx->judgement_delta.db->derivation_candidates,
					ctx->judgement_delta.db->derivation_candidate_count,
					(uint32_t)(i - 1),
					proof_kind,
					&(uint32_t){0}
				) == 0) {
				*p_classifier = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

/*
 * A source operation is a typed occurrence.  Its TermDB root can be shared
 * with an alpha-equivalent occurrence whose children have different raw
 * binder ids.  JudgementDB is serialized using TermDB representatives, so
 * reify the occurrence derivation against the representative's structural
 * children instead of assuming operation->core_term retains the source
 * child ids.
 */
static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->operation_count ||
		core_term >= ctx->terms->term_count || depth > 256) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	/* OperationGraph owns the typed occurrence. The proof conclusion must use
	 * that occurrence's own Core projection, not a structurally equal child
	 * recovered from an interned parent Term. */
	core_term = operation->core_term;
	prototype_judgement_delta_set_operation(
		&ctx->judgement_delta, operation_id
	);
	uint32_t classifier = operation->classifier;
	const struct prototype_term* term = &ctx->terms->terms[core_term];
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	int free_effect_row = operation_classifier_contains_unowned_effect_row(
		ctx, operation_id, classifier
	);
	if (free_effect_row < 0) {
		return -1;
	}
	if (free_effect_row != 0) {
		/* Recursive evidence construction must obey the same closure boundary as
		 * the outer materialization pass. An ascription or parent APP must not
		 * turn a residual external effect-row constraint into a kernel fact. */
		return 1;
	}
	if (ctx->judgement) {
		struct prototype_judgement_selected_evidence accepted_evidence;
		int accepted_status = prototype_judgement_select_evidence(
			ctx->judgement,
			operation_solver_evidence_owner(ctx, operation_id),
			operation->context_id,
			core_term,
			classifier,
			&accepted_evidence
		);
		if (accepted_status < 0 || accepted_status == 2) {
			return -1;
		}
		if (accepted_status == 0) {
			return 0;
		}
	}

	switch (operation->tag) {
		case PROTOTYPE_OPERATION_LAMBDA: {
			if (term->tag != PROTOTYPE_TERM_LAMBDA ||
				operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation->body];
			if (body->classifier == PROTOTYPE_INVALID_ID ||
				operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->body, body->core_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			const struct prototype_context* binder_context = prototype_context_get(
				&ctx->metadata->contexts, body->context_id
			);
			uint32_t binder_var;
			if (!binder_context || binder_context->parent != operation->context_id ||
				binder_context->binding_id == PROTOTYPE_INVALID_ID ||
				prototype_term_var(
					ctx->terms, binder_context->binding_id, &binder_var
				) != 0) {
				return -1;
			}
			int has_binder_assumption = 0;
			for (size_t i = 0;
				i < ctx->judgement_delta.proposition_count;
				++i) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[i];
				if (relation->context_id == body->context_id &&
					relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					relation->subject == binder_var &&
					relation->classifier == operation->binder_classifier &&
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)i,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) == 0) {
					has_binder_assumption = 1;
					break;
				}
			}
			if (!has_binder_assumption) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, body->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				size_t before = ctx->judgement_delta.proposition_count;
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						binder_context->binding_id,
						operation->binder_classifier
					) != 0 ||
					ctx->judgement_delta.proposition_count <= before) {
					return -1;
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			uint32_t expected_family;
			uint32_t expected_classifier;
			if (prototype_term_pure_family(
					ctx->terms,
					term->as.lambda.binding_id,
					body->classifier,
					&expected_family
				) != 0 || prototype_term_pi_family(
					ctx->terms,
					operation->binder_classifier,
					expected_family,
					&expected_classifier
				) != 0 || operation_solver_generalize_lambda_effect_rows(
					ctx, operation_id, expected_classifier, &expected_classifier
				) != 0) {
				return -1;
			}
			if (!(prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					classifier,
					expected_classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					return 1;
				}
				struct prototype_judgement_selected_evidence binder_evidence;
				int binder_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					body->context_id,
					binder_var,
					operation->binder_classifier,
					&binder_evidence
				);
				struct prototype_judgement_selected_evidence body_evidence;
				int body_status = binder_status == 0 ?
					operation_solver_select_evidence_in_context(
						ctx,
						operation_solver_evidence_owner(ctx, operation->body),
						body->context_id,
						body->core_term,
						body->classifier,
						&body_evidence
					) : binder_status;
				if (body_status != 0) {
					return body_status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, operation_id
				);
				if (prototype_judgement_delta_record_lambda_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation_id,
					core_term,
					classifier,
					binder_var,
					body->core_term,
						&binder_evidence,
						operation->body,
						&body_evidence
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_APP: {
			if (term->tag != PROTOTYPE_TERM_APP ||
				operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				uint32_t head;
				uint32_t owner;
				uint32_t constructor_index;
				uint32_t arguments[64];
				uint32_t argument_count;
				uint32_t reverse_argument_operations[64];
				uint32_t argument_operation_count = 0;
				uint32_t cursor = operation_id;
				if (prototype_term_constructor_spine_info(
						ctx->terms,
						core_term,
						&head,
						&owner,
						&constructor_index,
						arguments,
						64,
						&argument_count
					) != 0) {
					return -1;
				}
				while (cursor < ctx->metadata->operation_count) {
					const struct prototype_operation_node* cursor_operation =
						&ctx->metadata->operations[cursor];
					if (cursor_operation->tag != PROTOTYPE_OPERATION_APP ||
						cursor_operation->application_role !=
							PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
						break;
					}
					if (argument_operation_count >= 64 ||
						cursor_operation->argument >=
							ctx->metadata->operation_count) {
						return -1;
					}
					reverse_argument_operations[argument_operation_count++] =
						cursor_operation->argument;
					cursor = cursor_operation->function;
				}
				if (argument_operation_count != argument_count) {
					return -1;
				}
					uint32_t argument_operations[64];
					uint32_t argument_classifiers[64];
					struct prototype_judgement_selected_evidence argument_evidence[64];
				for (uint32_t i = 0; i < argument_count; ++i) {
					uint32_t argument_operation = reverse_argument_operations[
						argument_count - i - 1
					];
					argument_operations[i] = argument_operation;
					const struct prototype_operation_node* argument_operation_node =
						&ctx->metadata->operations[argument_operation];
					argument_classifiers[i] = argument_operation_node->classifier;
					if (argument_classifiers[i] == PROTOTYPE_INVALID_ID) {
						return 1;
					}
					int status = operation_solver_reify_core_proof(
						ctx,
						argument_operation,
						arguments[i],
						depth + 1
					);
					if (status != 0) {
						return status;
					}
						status = operation_solver_select_evidence_in_context(
							ctx,
						operation_solver_evidence_owner(ctx, argument_operation),
						operation->context_id,
						arguments[i],
							argument_classifiers[i],
							&argument_evidence[i]
					);
					if (status != 0) {
						return status;
					}
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, operation_id
				);
				(void)head;
				(void)owner;
				(void)constructor_index;
				return prototype_judgement_delta_record_constructor_spine(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
						classifier,
						argument_operations,
						argument_evidence,
					argument_count
				);
			}
			const struct prototype_operation_node* function =
				&ctx->metadata->operations[operation->function];
			const struct prototype_operation_node* argument =
				&ctx->metadata->operations[operation->argument];
			if (function->classifier == PROTOTYPE_INVALID_ID ||
				argument->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->function, term->as.app.function, depth + 1
			);
			if (status != 0) {
				return status;
			}
			status = operation_solver_reify_core_proof(
				ctx, operation->argument, term->as.app.argument, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence function_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->function),
				operation->context_id,
				term->as.app.function,
				function->classifier,
				&function_evidence
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence argument_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->argument),
				operation->context_id,
				term->as.app.argument,
				argument->classifier,
				&argument_evidence
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			if (prototype_judgement_delta_record_app_elim(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					&function_evidence,
					&argument_evidence
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE: {
			uint32_t child_term;
			if (operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			if (term->tag == PROTOTYPE_TERM_RETURN) {
				child_term = term->as.return_term.value;
			} else if (term->tag == PROTOTYPE_TERM_THUNK) {
				child_term = term->as.thunk.computation;
			} else if (term->tag == PROTOTYPE_TERM_FORCE) {
				child_term = term->as.force.value;
			} else {
				return -1;
			}
			uint32_t child_classifier = ctx->metadata->operations[
				operation->argument
			].classifier;
			if (child_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->argument, child_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence child_evidence;
			status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->argument),
				operation->context_id,
				child_term,
				child_classifier,
				&child_evidence
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			uint32_t derived_classifier;
			if (prototype_judgement_cbpv_boundary_classifier(
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_classifier,
					&derived_classifier
				) != 0 || prototype_judgement_delta_record_cbpv_boundary(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					operation->argument,
					&child_evidence
				) != 0) {
				return -1;
			}
			if (derived_classifier != classifier) {
				struct prototype_judgement_selected_evidence boundary_evidence;
				if (operation_solver_delta_select_evidence(
						ctx,
						operation_id,
						operation->context_id,
						core_term,
						derived_classifier,
						&boundary_evidence
					) != 0 ||
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&boundary_evidence,
						classifier,
						core_term
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD: {
			if (term->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
				operation->function >= ctx->metadata->operation_count) {
				return -1;
			}
			uint32_t proven_classifier;
			int proven_status = operation_solver_lookup_operation_derivation_classifier(
				ctx,
				operation_id,
				core_term,
				PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
				&proven_classifier
			);
			if (proven_status < 0) {
				return -1;
			}
			if (proven_status == 0) {
				int evidence_status = operation_solver_require_evidence_in_context(
					ctx,
					operation_id,
					operation->context_id,
					core_term,
					proven_classifier
				);
				return evidence_status;
			}
			uint32_t clause_count = term->as.computation_fold.clause_count;
			if (clause_count != 0) {
				/* Clause-bearing folds are kernel solver conclusions. The
				 * OperationGraph classifier is only a fixed-point approximation. */
				return 1;
			}
			if (clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
				operation->fold_clause_count != clause_count ||
				(clause_count != 0 &&
				 (operation->first_fold_clause >
					ctx->metadata->operation_fold_clause_count ||
				  clause_count > ctx->metadata->operation_fold_clause_count -
					operation->first_fold_clause ||
				  term->as.computation_fold.first_clause >
					ctx->terms->computation_fold_clause_count ||
				  clause_count > ctx->terms->computation_fold_clause_count -
					term->as.computation_fold.first_clause))) {
				return -1;
			}
			const struct prototype_operation_node* computation =
				&ctx->metadata->operations[operation->function];
			uint32_t return_operation_id = clause_count == 0 ?
				operation->argument : operation->fold_return_operation;
			if (return_operation_id >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* return_operation =
				&ctx->metadata->operations[return_operation_id];
			uint32_t premise_count = 2 + 2 * clause_count;
				uint32_t premise_operations[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				struct prototype_judgement_selected_evidence premise_evidence[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
			premise_operations[0] = operation->function;
			premise_operations[1] = return_operation_id;
			premise_terms[0] = term->as.computation_fold.computation;
			premise_terms[1] = term->as.computation_fold.return_clause;
			premise_classifiers[0] = computation->classifier;
			premise_classifiers[1] = return_operation->classifier;
			if (computation->tag == PROTOTYPE_OPERATION_REQUEST) {
				int proven_status = operation_solver_lookup_operation_derivation_classifier(
					ctx,
					operation->function,
					premise_terms[0],
					PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
					&premise_classifiers[0]
				);
				if (proven_status != 0) {
					return proven_status < 0 ? -1 : 1;
				}
			}
			for (uint32_t i = 0; i < clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* occurrence_clause =
					&ctx->metadata->operation_fold_clauses[
						operation->first_fold_clause + i
					];
				const struct prototype_computation_fold_clause* core_clause =
					&ctx->terms->computation_fold_clauses[
						term->as.computation_fold.first_clause + i
					];
				premise_operations[2 + 2 * i] =
					occurrence_clause->operation_operation;
				premise_operations[3 + 2 * i] =
					occurrence_clause->clause_operation;
				premise_terms[2 + 2 * i] = core_clause->operation;
				premise_terms[3 + 2 * i] = core_clause->body;
			}
			for (uint32_t i = 2; i < premise_count; ++i) {
				if (premise_operations[i] >= ctx->metadata->operation_count) {
					return -1;
				}
				premise_classifiers[i] = ctx->metadata->operations[
					premise_operations[i]
				].classifier;
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				if (premise_classifiers[i] == PROTOTYPE_INVALID_ID) {
					return 1;
				}
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				int status = operation_solver_reify_core_proof(
					ctx,
					premise_operations[i],
					premise_terms[i],
					depth + 1
				);
				if (status != 0) {
					return status;
				}
					if (clause_count == 0) {
						status = operation_solver_select_evidence_in_context(
							ctx,
						operation_solver_evidence_owner(
							ctx, premise_operations[i]
						),
						operation->context_id,
						premise_terms[i],
							premise_classifiers[i],
							&premise_evidence[i]
					);
					if (status != 0) {
						return status;
					}
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			int status = prototype_judgement_delta_record_computation_fold_elim(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
					core_term,
						classifier,
						premise_operations,
						premise_evidence,
				premise_count
			);
			return status < 0 ? -1 : 0;
		}
		case PROTOTYPE_OPERATION_VAR: {
			if (term->tag != PROTOTYPE_TERM_VAR) {
				return -1;
			}
			int status = operation_solver_require_evidence_in_context(
				ctx,
				PROTOTYPE_INVALID_ID,
				operation->context_id,
				core_term,
				classifier
			);
			if (status <= 0) {
				return status;
			}
			for (uint32_t lambda_operation_id = 0;
				lambda_operation_id < ctx->metadata->operation_count;
				++lambda_operation_id) {
				const struct prototype_operation_node* lambda_operation =
					&ctx->metadata->operations[lambda_operation_id];
				uint8_t visited[4096];
				if (lambda_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
					lambda_operation->referenced_ast_binder_id !=
						operation->referenced_ast_binder_id ||
					lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						lambda_operation->binder_classifier,
						classifier
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				memset(visited, 0, sizeof(visited));
				if (operation_subtree_contains_operation(
						ctx,
						lambda_operation->body,
						operation_id,
						visited
					) <= 0 ||
					lambda_operation->body >= ctx->metadata->operation_count) {
					continue;
				}
				uint32_t binder_context =
					ctx->metadata->operations[lambda_operation->body].context_id;
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, binder_context
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->terms->terms[core_term].as.var.binding_id,
						classifier
					) != 0) {
					return -1;
				}
				status = operation_solver_require_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					operation->context_id,
					core_term,
					classifier
				);
				return status == 0 ? 0 : status;
			}
			return 1;
		}
		case PROTOTYPE_OPERATION_NAME: {
			if (operation->function >= ctx->metadata->operation_count) {
				return -1;
			}
			int child_status = operation_solver_reify_core_proof(
				ctx, operation->function, core_term, depth + 1
			);
			if (child_status != 0) {
				return child_status;
			}
			return operation_solver_require_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation->function),
				operation->context_id,
				core_term,
				classifier
			);
		}
		case PROTOTYPE_OPERATION_MATCH: {
			if (term->tag != PROTOTYPE_TERM_MATCH ||
				operation->scrutinee >= ctx->metadata->operation_count ||
				operation->first_case + operation->case_count >
					ctx->metadata->operation_case_count ||
				term->as.match.case_count != operation->case_count) {
				return -1;
			}
			int status = operation_solver_reify_core_proof(
				ctx,
				operation->scrutinee,
				term->as.match.scrutinee,
				depth + 1
			);
			if (status != 0) {
				return status;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				const struct prototype_operation_match_case* operation_case =
					&ctx->metadata->operation_cases[operation->first_case + i];
				if (case_id >= ctx->terms->case_count ||
					operation_case->body_operation >=
						ctx->metadata->operation_count) {
					return -1;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					operation_case->body_operation,
					ctx->terms->cases[case_id].body,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
			}
			return operation_solver_require_evidence_in_context(
				ctx,
				operation_id,
				operation->context_id,
				core_term,
				classifier
			);
		}
		case PROTOTYPE_OPERATION_ASCRIPTION: {
			if (operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation->body];
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->body, body->core_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			struct prototype_judgement_selected_evidence source_evidence;
			if (operation_solver_delta_select_evidence(
					ctx,
					operation_solver_evidence_owner(ctx, operation->body),
					body->context_id,
					core_term,
					body->classifier,
					&source_evidence
				) != 0) {
				return -1;
			}
			return prototype_judgement_delta_record_expected_type_exposure(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				&source_evidence,
				classifier,
				core_term
			);
		}
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			return 0;
		case PROTOTYPE_OPERATION_REQUEST:
			if (term->tag != PROTOTYPE_TERM_OPERATION_REQUEST ||
				operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count) {
				return -1;
			}
			{
				const struct prototype_operation_node* function =
					&ctx->metadata->operations[operation->function];
				const struct prototype_operation_node* argument =
					&ctx->metadata->operations[operation->argument];
				if (function->classifier == PROTOTYPE_INVALID_ID ||
					argument->classifier == PROTOTYPE_INVALID_ID) {
					return 1;
				}
				int status = operation_solver_reify_core_proof(
					ctx,
					operation->function,
					term->as.operation_request.operation,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					operation->argument,
					term->as.operation_request.argument,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				uint32_t application;
				if (prototype_term_app(
						ctx->terms,
						term->as.operation_request.operation,
						term->as.operation_request.argument,
						&application
					) != 0) {
					return -1;
				}
				struct prototype_judgement_selected_evidence function_evidence;
				status = operation_solver_select_evidence_in_context(
					ctx,
					operation_solver_evidence_owner(ctx, operation->function),
					operation->context_id,
					term->as.operation_request.operation,
					function->classifier,
					&function_evidence
				);
				if (status != 0) {
					return status;
				}
				struct prototype_judgement_selected_evidence argument_evidence;
				status = operation_solver_select_evidence_in_context(
					ctx,
					operation_solver_evidence_owner(ctx, operation->argument),
					operation->context_id,
					term->as.operation_request.argument,
					argument->classifier,
					&argument_evidence
				);
				if (status != 0) {
					return status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_operation(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				uint32_t application_classifier;
				if (prototype_judgement_delta_app_elim_classifier(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						function->classifier,
						term->as.operation_request.argument,
						argument->classifier,
						&application_classifier
					) != 0 || prototype_judgement_delta_record_app_elim(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						application,
						application_classifier,
						&function_evidence,
						&argument_evidence
					) != 0) {
					return -1;
				}
			}
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, operation_id
			);
			return prototype_judgement_delta_record_computation_constraint(
				&ctx->judgement_delta,
				ctx->terms,
				operation->context_id,
				core_term
			);
		default:
			return -1;
	}
}

static int operation_solver_validate_solution_projection(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[i];
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		if (solution->classifier != operation->classifier) {
			return -1;
		}
		if (solution->classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				solution->classifier,
				&view
			) != 0) {
			return -1;
		}
		if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
			solution->effect_row != view.effect_row) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_materialize_judgements(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	if (operation_solver_validate_solution_projection(ctx) != 0) {
		fprintf(stderr, "judgement materialization: invalid solution projection\n");
		return -1;
	}
	if (materialize_pending_binder_assumptions(ctx) != 0 ||
		materialize_pending_declaration_facts(ctx) != 0) {
		fprintf(stderr, "judgement materialization: pending input facts failed\n");
		return -1;
	}
	if (operation_solver_materialize_match_pattern_assumptions(ctx) != 0) {
		fprintf(stderr, "judgement materialization: match assumptions failed\n");
		return -1;
	}
	for (;;) {
		size_t before_relation_count = ctx->judgement_delta.proposition_count;
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		if (operation->tag != PROTOTYPE_OPERATION_MATCH) {
			continue;
		}
		if (!operation_match_cases_are_resolved(ctx, operation)) {
			continue;
		}
		/* A Match classifier is solved from its branch computations.  In the
		 * first fixed-point round RETURN/THUNK/COMPUTATION_FOLD may not yet have supplied
		 * those branch classifiers, so an unresolved Match is a deferred
		 * equation, not a compilation error. */
		if (operation->classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, operation->context_id
		);
		prototype_judgement_delta_set_operation(
			&ctx->judgement_delta, i
		);
		uint32_t branch_operation_ids[64];
		struct prototype_judgement_selected_evidence branch_evidence[64];
		if (operation->case_count > 64) {
			return -1;
		}
		int branches_ready = 1;
		for (uint32_t case_index = 0;
			case_index < operation->case_count;
			++case_index) {
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[operation->first_case + case_index];
			if (operation_case->body_operation >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* body =
				&ctx->metadata->operations[operation_case->body_operation];
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				branches_ready = 0;
				break;
			}
			branch_operation_ids[case_index] = operation_case->body_operation;
			int evidence_status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(
					ctx, operation_case->body_operation
				),
				body->context_id,
				body->core_term,
				body->classifier,
				&branch_evidence[case_index]
			);
			if (evidence_status < 0) {
				return -1;
			}
			if (evidence_status != 0) {
				branches_ready = 0;
				break;
			}
		}
		if (!branches_ready) {
			continue;
		}
		if (prototype_judgement_delta_record_materialized_match_motive(
				&ctx->judgement_delta,
				ctx->terms,
				operation->core_term,
				operation->classifier,
				branch_operation_ids,
				branch_evidence,
				operation->case_count
			) != 0) {
			return -1;
		}
	}
		for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[i];
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_operation(
				&ctx->judgement_delta, i
			);
			uint32_t classifier = operation->classifier;
			if (classifier == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int free_effect_row = operation_classifier_contains_unowned_effect_row(
				ctx, i, classifier
			);
			if (free_effect_row < 0) {
				return -1;
			}
			if (free_effect_row != 0) {
				/* The OperationGraph may retain this constraint state, but a
				 * JudgementDB conclusion must be closed or explicitly generalized. */
				continue;
			}
			if (operation->tag == PROTOTYPE_OPERATION_LAMBDA ||
				operation->tag == PROTOTYPE_OPERATION_APP ||
				operation->tag == PROTOTYPE_OPERATION_ASCRIPTION ||
				operation->tag == PROTOTYPE_OPERATION_RETURN ||
				operation->tag == PROTOTYPE_OPERATION_THUNK ||
				operation->tag == PROTOTYPE_OPERATION_FORCE ||
				operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
				operation->tag == PROTOTYPE_OPERATION_REQUEST) {
				int reify_status = operation_solver_reify_core_proof(
					ctx, i, operation->core_term, 0
				);
				if (reify_status < 0) {
					fprintf(
						stderr,
						"judgement materialization: reify failed operation=%u tag=%d\n",
						i,
						operation->tag
					);
					return -1;
				}
				if (reify_status > 0) {
					continue;
				}
			} else if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
				if (operation_solver_materialize_induction_hypothesis_judgement(
						ctx, i
					) != 0) {
					return -1;
				}
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				prototype_term_type_instance_info(
					ctx->terms,
					operation->core_term,
					&(uint32_t){0},
					NULL,
					&(uint32_t){0}
				) == 0 &&
				prototype_judgement_delta_record_type_formation(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_PURE_PRIMITIVE &&
				prototype_judgement_delta_record_pure_primitive_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_EFFECT_OPERATION &&
				prototype_judgement_delta_record_effect_operation_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					operation->classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_TEXT_LITERAL &&
				prototype_judgement_delta_record_text_literal(
					&ctx->judgement_delta,
					ctx->terms,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_OPERATION_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_INT_LITERAL) {
				if (prototype_judgement_delta_record_int_literal(
						&ctx->judgement_delta,
						ctx->terms,
						operation->core_term,
						classifier
					) != 0) {
					return -1;
				}
				int64_t value = ctx->terms->terms[
					operation->core_term
				].as.int_literal.value;
				if (value >= INT32_MIN && value <= INT32_MAX) {
					uint32_t integer;
					struct prototype_judgement_selected_evidence source_evidence;
					if (prototype_term_make_host_type(
								ctx->terms,
								PROTOTYPE_HOST_TYPE_INT32,
								&integer
							) != 0 || operation_solver_delta_select_evidence(
								ctx,
								i,
								operation->context_id,
								operation->core_term,
								classifier,
								&source_evidence
							) != 0 ||
							prototype_judgement_delta_record_int_literal_admissibility(
								&ctx->judgement_delta,
								ctx->terms,
								&source_evidence,
								integer
							) != 0) {
							return -1;
						}
					}
					}
			}
			/* Computation constraints are generated and solved before this phase, but
			 * their replayable derivations require premise evidence materialized above.
			 * Re-run only the checked rule closure; operation solutions are validated
			 * again and are never selected here. */
			if (operation_solver_refresh_computation_constraint_operands(ctx) != 0 ||
				prototype_judgement_delta_solve_recorded_computation_requests(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations
				) != 0 ||
				prototype_judgement_delta_solve_recorded_computation_constraints(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations
				) != 0 || operation_solver_validate_solution_projection(ctx) != 0) {
				return -1;
			}
			if (ctx->judgement_delta.proposition_count == before_relation_count) {
				break;
		}
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		struct prototype_judgement_derivation_candidate* derivation =
			&ctx->judgement_delta.derivation_candidates[i];
		for (uint32_t premise_index = 0;
			premise_index < derivation->premise_count;
			++premise_index) {
			struct prototype_judgement_proposition* premise =
				&derivation->premises[premise_index].proposition;
			if (premise->operation_id != PROTOTYPE_INVALID_ID) {
				struct prototype_usage_vector usage;
				if (operation_usage_solution_view(
						ctx, premise->operation_id, &usage
					) != 0 || usage.count > UINT32_MAX) {
					return -1;
				}
				premise->resource_usage_count = (uint32_t)usage.count;
				premise->resource_usage = usage.entries;
				continue;
			}
			for (size_t proposition_index = 0;
				proposition_index < ctx->judgement_delta.proposition_count;
				++proposition_index) {
				const struct prototype_judgement_proposition* available =
					&ctx->judgement_delta.propositions[proposition_index];
				if (available->kind == premise->kind &&
					available->context_id == premise->context_id &&
					available->subject == premise->subject &&
					available->classifier == premise->classifier &&
					available->authority_kind == premise->authority_kind &&
					available->authority_id == premise->authority_id) {
					premise->resource_usage_count =
						available->resource_usage_count;
					premise->resource_usage = available->resource_usage;
					break;
				}
			}
		}
	}
	return 0;
}

static int operation_solver_resolve_contexts(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->contexts.context_count > PROTOTYPE_CONTEXT_CAPACITY) {
		return -1;
	}
	uint32_t relocation[PROTOTYPE_CONTEXT_CAPACITY];
	size_t source_count = ctx->metadata->contexts.context_count;
	relocation[0] = 0;
	for (uint32_t context_id = 1; context_id < source_count; ++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context || context->parent >= context_id) {
			return -1;
		}
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		int classifier_variable_resolved = 0;
		if (classifier_variable != PROTOTYPE_INVALID_ID) {
			/* A Lambda owns the classifier of the Context introduced for its body.
			 * VAR occurrences with the same surface binder can be elaborated through
			 * different CBPV boundaries, so they are not authority for this choice. */
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->operation_count;
				++operation_id) {
				const struct prototype_operation_node* operation =
					&ctx->metadata->operations[operation_id];
				if (operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
					operation->body >= ctx->metadata->operation_count ||
					ctx->metadata->operations[operation->body].context_id != context_id ||
					operation->binding_id != context->binding_id ||
					operation->binder_classifier == PROTOTYPE_INVALID_ID) {
					continue;
				}
				if (classifier_variable_resolved &&
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						operation->binder_classifier
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					return -1;
				}
				classifier = operation->binder_classifier;
				classifier_variable_resolved = 1;
			}
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->operation_count;
				++operation_id) {
				const struct prototype_operation_node* operation =
					&ctx->metadata->operations[operation_id];
				uint32_t operation_classifier =
					operation_available_classifier(operation);
				if (!classifier_variable_resolved &&
					operation->tag == PROTOTYPE_OPERATION_VAR &&
					operation->referenced_ast_binder_id ==
						classifier_variable &&
					operation_classifier != PROTOTYPE_INVALID_ID) {
					classifier = operation_classifier;
					classifier_variable_resolved = 1;
				}
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
		if (prototype_context_extend(
				&ctx->metadata->contexts,
				relocation[context->parent],
				context->binding_id,
				classifier,
				finalize && classifier_variable_resolved ?
					PROTOTYPE_INVALID_ID : classifier_variable,
				&relocation[context_id]
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		uint32_t context_id = ctx->metadata->operations[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operations[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_case_count; ++i) {
		uint32_t context_id = ctx->metadata->operation_cases[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operation_cases[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_fold_clause_count; ++i) {
		uint32_t context_id = ctx->metadata->operation_fold_clauses[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->metadata->operation_fold_clauses[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		uint32_t context_id = ctx->pending_binder_assumptions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->pending_binder_assumptions[i].context_id = relocation[context_id];
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		uint32_t context_id = ctx->pending_declaration_facts[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		ctx->pending_declaration_facts[i].context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement_delta.propositions[i];
		uint32_t context_id = proposition->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		proposition->context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement_delta.derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof =
			&ctx->judgement_delta.derivation_candidates[i];
		if (proof->conclusion_context_id >= source_count) {
			return -1;
		}
		proof->conclusion_context_id =
			relocation[proof->conclusion_context_id];
		if (proof->conclusion_proposition_id <
				ctx->judgement_delta.proposition_count) {
			const struct prototype_judgement_proposition* conclusion =
				&ctx->judgement_delta.propositions[
					proof->conclusion_proposition_id
				];
			proof->conclusion_context_id = conclusion->context_id;
			proof->conclusion_classifier = conclusion->classifier;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (proof->premises[j].proposition.context_id >= source_count) {
				return -1;
			}
			proof->premises[j].proposition.context_id =
				relocation[proof->premises[j].proposition.context_id];
		}
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			(proof->premise_count != 1 ||
			 prototype_substitution_projection_path(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				proof->conclusion_context_id,
				proof->premises[0].proposition.context_id,
				&proof->semantic_action_id
			 ) != 0)) {
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
		ctx->judgement_delta.computation_constraints[i].context_id =
			relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement->proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement->propositions[i];
		uint32_t context_id = proposition->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		proposition->context_id = relocation[context_id];
	}
	for (size_t i = 0; i < ctx->judgement->derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof = &ctx->judgement->derivation_candidates[i];
		if (proof->conclusion_context_id >= source_count) {
			return -1;
		}
		proof->conclusion_context_id =
			relocation[proof->conclusion_context_id];
		if (proof->conclusion_proposition_id < ctx->judgement->proposition_count) {
			const struct prototype_judgement_proposition* conclusion =
				&ctx->judgement->propositions[proof->conclusion_proposition_id];
			proof->conclusion_context_id = conclusion->context_id;
			proof->conclusion_classifier = conclusion->classifier;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (proof->premises[j].proposition.context_id >= source_count) {
				return -1;
			}
			proof->premises[j].proposition.context_id =
				relocation[proof->premises[j].proposition.context_id];
		}
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			(proof->premise_count != 1 ||
			 prototype_substitution_projection_path(
				&ctx->metadata->substitutions,
				&ctx->metadata->contexts,
				proof->conclusion_context_id,
				proof->premises[0].proposition.context_id,
				&proof->semantic_action_id
			 ) != 0)) {
			return -1;
		}
	}
	if (prototype_context_db_rebuild_index(
			&ctx->metadata->contexts
		) != 0 || prototype_substitution_db_rebuild_index(
			&ctx->metadata->substitutions
		) != 0) {
		return -1;
	}
	return 0;
}

static int operation_solver_generate_computation_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
			operation->tag != PROTOTYPE_OPERATION_REQUEST) {
			continue;
		}
		prototype_judgement_delta_set_operation(
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
		struct prototype_judgement_computation_constraint* constraint = NULL;
		for (size_t constraint_id = 0;
			constraint_id < ctx->judgement_delta.computation_constraint_count;
			++constraint_id) {
			struct prototype_judgement_computation_constraint* candidate =
				&ctx->judgement_delta.computation_constraints[constraint_id];
			if (candidate->operation_id == i &&
				candidate->subject == operation->core_term) {
				constraint = candidate;
				break;
			}
		}
		if (!constraint) {
			return -1;
		}
		constraint->premise_operation_count = 0;
		constraint->return_body_operation_id = PROTOTYPE_INVALID_ID;
		constraint->return_body_context_id = PROTOTYPE_INVALID_ID;
		constraint->return_body_subject = PROTOTYPE_INVALID_ID;
		for (uint32_t premise = 0;
			premise < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES;
			++premise) {
			constraint->premise_contexts[premise] = PROTOTYPE_INVALID_ID;
			constraint->premise_states[premise] =
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
		if (operation->tag == PROTOTYPE_OPERATION_REQUEST) {
			if (operation->function >= ctx->metadata->operation_count ||
				operation->argument >= ctx->metadata->operation_count ||
				operation->body >= ctx->metadata->operation_count) {
				return -1;
			}
			constraint->premise_operations[0] = operation->function;
			constraint->premise_operations[1] = operation->argument;
			constraint->premise_operations[2] = operation->body;
			constraint->premise_operation_count = 3;
			continue;
		}
		uint32_t return_operation = operation->fold_clause_count == 0 ?
			operation->argument : operation->fold_return_operation;
		uint32_t premise_count = 2 + 2 * operation->fold_clause_count;
		if (operation->function >= ctx->metadata->operation_count ||
			return_operation >= ctx->metadata->operation_count || premise_count > 64 ||
			(operation->fold_clause_count != 0 &&
			 ((size_t)operation->first_fold_clause + operation->fold_clause_count >
				ctx->metadata->operation_fold_clause_count))) {
			return -1;
		}
		constraint->premise_operations[0] = operation->function;
		constraint->premise_operations[1] = return_operation;
		if (ctx->metadata->operations[return_operation].tag ==
				PROTOTYPE_OPERATION_LAMBDA &&
			ctx->metadata->operations[return_operation].body <
				ctx->metadata->operation_count) {
			constraint->return_body_operation_id =
				ctx->metadata->operations[return_operation].body;
			constraint->return_body_context_id = ctx->metadata->operations[
				constraint->return_body_operation_id
			].context_id;
			constraint->return_body_subject = ctx->metadata->operations[
				constraint->return_body_operation_id
			].core_term;
		}
		for (uint32_t clause = 0; clause < operation->fold_clause_count; ++clause) {
			const struct prototype_operation_computation_fold_clause* fold_clause =
				&ctx->metadata->operation_fold_clauses[
					operation->first_fold_clause + clause
				];
			constraint->premise_operations[2 + 2 * clause] =
				fold_clause->operation_operation;
			constraint->premise_operations[3 + 2 * clause] =
				fold_clause->clause_operation;
		}
		constraint->premise_operation_count = premise_count;
	}
	return 0;
}

static int operation_solver_bind_computation_constraint_solutions(
	struct compile_context* ctx,
	int* p_changed
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
		int is_request = constraint->kind ==
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		if ((pass == 0) != is_request) {
			continue;
		}
		if (constraint->operation_id == PROTOTYPE_INVALID_ID ||
			constraint->solved_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		/* Solutions are derived from the premise-classifier snapshot populated
		 * before this binding pass. Earlier solutions in this same pass can
		 * refine an operation used by a later constraint. Do not publish the
		 * dependent stale solution; the next fixed-point round will refresh and
		 * solve that constraint from the refined premise. */
		int stale = 0;
		for (uint32_t premise = 0;
			premise < constraint->premise_operation_count;
			++premise) {
			uint32_t operation_id = constraint->premise_operations[premise];
			if (operation_id >= ctx->metadata->operation_count) {
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
		if (constraint->operation_id >= ctx->metadata->operation_count ||
			constraint->solved_classifier >= ctx->terms->term_count ||
			operation_solver_bind_proven_classifier(
				ctx,
				constraint->operation_id,
				constraint->solved_classifier,
				p_changed
			) != 0) {
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
		if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
			if (constraint->operation_id >= ctx->metadata->operation_count) {
				return -1;
			}
			constraint->projected_classifier = operation_solver_classifier(
				ctx, constraint->operation_id
			);
		}
		if (constraint->premise_operation_count > 64) {
			return -1;
		}
		for (uint32_t premise = 0;
			premise < constraint->premise_operation_count;
			++premise) {
			uint32_t operation_id = constraint->premise_operations[premise];
			if (operation_id >= ctx->metadata->operation_count) {
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
				ctx->metadata->operations[operation_id].context_id;
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
			int evidence_status = operation_solver_select_evidence_in_context(
				ctx,
				operation_solver_evidence_owner(ctx, operation_id),
				constraint->context_id,
				ctx->metadata->operations[operation_id].core_term,
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
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if ((operation->tag != PROTOTYPE_OPERATION_RETURN &&
			 operation->tag != PROTOTYPE_OPERATION_THUNK &&
			 operation->tag != PROTOTYPE_OPERATION_FORCE) ||
			operation->argument >= ctx->metadata->operation_count) {
			continue;
		}
		uint32_t child_classifier = operation_solver_classifier(
			ctx, operation->argument
		);
		if (child_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		if (prototype_judgement_cbpv_boundary_classifier(
				ctx->terms,
				ctx->type_declarations,
				operation->core_term,
				child_classifier,
				&classifier
			) != 0 || operation_solver_bind_proven_classifier(
				ctx, operation_id, classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	return operation_solver_commit_bindings(ctx, p_changed);
}
