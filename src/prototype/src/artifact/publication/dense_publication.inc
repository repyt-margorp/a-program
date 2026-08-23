struct artifact_closure_graph {
	struct prototype_term_db terms;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement;
	struct prototype_universe_db universe;
	struct prototype_term* term_nodes;
	struct prototype_match_case* cases;
	int* case_label_symbols;
	struct prototype_case_binder* case_binders;
	struct prototype_ih_scope* frames;
	struct prototype_type_declaration* type_nodes;
	struct prototype_type_parameter_declaration* parameter_declarations;
	struct prototype_type_constructor_declaration* constructor_declarations;
	struct prototype_type_constructor_readback* constructor_readbacks;
	struct prototype_constructor_classifier_cache_entry*
		constructor_classifier_cache_entries;
	uint32_t* readback_field_types;
	struct prototype_type_expr* type_exprs;
	struct prototype_type_representation* type_representations;
	struct prototype_judgement_proposition* propositions;
	struct prototype_judgement_derivation_candidate* derivation_candidates;
	struct prototype_judgement_claim* claims;
	struct prototype_judgement_derivation* derivations;
	struct prototype_judgement_candidate_premise* candidate_premises;
	struct prototype_judgement_premise_edge* accepted_premises;
	struct prototype_usage_entry* proposition_resource_usage;
	struct prototype_universe_node* universe_nodes;
	struct prototype_universe_edge* universe_edges;
	struct prototype_universe_level* universe_levels;
	struct prototype_universe_constraint* universe_constraints;
	uint32_t* term_order;
	uint32_t* case_order;
	uint32_t* case_binder_order;
	uint32_t* frame_order;
	uint32_t* type_order;
	uint32_t* parameter_order;
	uint32_t* constructor_order;
	uint32_t* readback_field_type_order;
	uint32_t* type_expr_order;
	uint32_t* proposition_order;
	uint32_t* claim_order;
	uint32_t* derivation_order;
	uint32_t* substitution_order;
	uint32_t* substitution_evidence_claims;
	size_t ordered_term_count;
	size_t ordered_case_count;
	size_t ordered_case_binder_count;
	size_t ordered_frame_count;
	size_t ordered_type_count;
	size_t ordered_parameter_count;
	size_t ordered_constructor_count;
	size_t ordered_readback_field_type_count;
	size_t ordered_type_expr_count;
	size_t ordered_proposition_count;
	size_t ordered_claim_count;
	size_t ordered_derivation_count;
	size_t ordered_substitution_count;
};

struct artifact_publication_graph {
	struct artifact_closure_graph object;
	struct prototype_artifact_interface interface;
	struct prototype_compile_metadata metadata;
	struct prototype_artifact_term_export* term_exports;
	struct prototype_artifact_type_export* type_exports;
	struct prototype_artifact_type_parameter_export* type_parameters;
	struct prototype_artifact_constructor_export* constructor_exports;
	uint32_t* constructor_field_type_exprs;
	struct prototype_type_expr* interface_type_exprs;
	struct prototype_artifact_identity_root* identity_roots;
	struct prototype_artifact_dependency* dependencies;
	struct prototype_context* contexts;
	struct prototype_substitution* substitutions;
	struct prototype_dimension_operator* dimension_operators;
	struct prototype_dimension_axis_image* dimension_images;
	struct prototype_typed_occurrence* occurrences;
	struct prototype_typed_occurrence_edge* occurrence_edges;
	struct prototype_typed_occurrence_match_case* occurrence_match_cases;
	struct prototype_typed_occurrence_fold_clause* occurrence_fold_clauses;
	struct prototype_verification_obligation* verification_obligations;
	uint32_t* term_relocation;
	uint32_t* binding_relocation;
	uint32_t* context_relocation;
	uint32_t* type_relocation;
	uint32_t* type_expr_relocation;
	uint32_t* parameter_relocation;
	uint32_t* constructor_relocation;
	uint32_t* field_relocation;
	uint32_t* proposition_relocation;
	uint32_t* claim_relocation;
	uint32_t* substitution_relocation;
	uint32_t* dimension_operator_relocation;
	uint32_t* substitution_evidence_claims;
};

static void artifact_closure_graph_free(struct artifact_closure_graph* graph) {
	if (!graph) {
		return;
	}
	free(graph->term_nodes);
	free(graph->cases);
	free(graph->case_label_symbols);
	free(graph->case_binders);
	free(graph->frames);
	free(graph->type_nodes);
	free(graph->parameter_declarations);
	free(graph->constructor_declarations);
	free(graph->constructor_readbacks);
	free(graph->constructor_classifier_cache_entries);
	free(graph->readback_field_types);
	free(graph->type_exprs);
	free(graph->type_representations);
	free(graph->claims);
	free(graph->derivations);
	free(graph->propositions);
	free(graph->derivation_candidates);
	free(graph->candidate_premises);
	free(graph->accepted_premises);
	free(graph->proposition_resource_usage);
	free(graph->universe_nodes);
	free(graph->universe_edges);
	free(graph->universe_levels);
	free(graph->universe_constraints);
	free(graph->term_order);
	free(graph->case_order);
	free(graph->case_binder_order);
	free(graph->frame_order);
	free(graph->type_order);
	free(graph->parameter_order);
	free(graph->constructor_order);
	free(graph->readback_field_type_order);
	free(graph->type_expr_order);
	free(graph->proposition_order);
	free(graph->claim_order);
	free(graph->derivation_order);
	free(graph->substitution_order);
	free(graph->substitution_evidence_claims);
	memset(graph, 0, sizeof(*graph));
}

static void artifact_closure_graph_take_publication_order(
	struct artifact_closure_graph* graph,
	struct artifact_graph_marks* marks
) {
	if (!graph || !marks) {
		return;
	}
#define TAKE_ORDER(field, count_field) \
	do { \
		graph->field = marks->field; \
		graph->count_field = marks->count_field; \
		marks->field = NULL; \
		marks->count_field = 0; \
	} while (0)
	TAKE_ORDER(term_order, ordered_term_count);
	TAKE_ORDER(case_order, ordered_case_count);
	TAKE_ORDER(case_binder_order, ordered_case_binder_count);
	TAKE_ORDER(frame_order, ordered_frame_count);
	TAKE_ORDER(type_order, ordered_type_count);
	TAKE_ORDER(parameter_order, ordered_parameter_count);
	TAKE_ORDER(constructor_order, ordered_constructor_count);
	TAKE_ORDER(readback_field_type_order, ordered_readback_field_type_count);
	TAKE_ORDER(type_expr_order, ordered_type_expr_count);
	TAKE_ORDER(proposition_order, ordered_proposition_count);
	TAKE_ORDER(claim_order, ordered_claim_count);
	TAKE_ORDER(derivation_order, ordered_derivation_count);
	TAKE_ORDER(substitution_order, ordered_substitution_count);
	graph->substitution_evidence_claims = marks->substitution_evidence_claims;
	marks->substitution_evidence_claims = NULL;
#undef TAKE_ORDER
}

static void artifact_publication_graph_free(
	struct artifact_publication_graph* graph
) {
	if (!graph) {
		return;
	}
	artifact_closure_graph_free(&graph->object);
	free(graph->term_exports);
	free(graph->type_exports);
	free(graph->type_parameters);
	free(graph->constructor_exports);
	free(graph->constructor_field_type_exprs);
	free(graph->interface_type_exprs);
	free(graph->identity_roots);
	free(graph->dependencies);
	free(graph->contexts);
	free(graph->substitutions);
	free(graph->dimension_operators);
	free(graph->dimension_images);
	free(graph->occurrences);
	free(graph->occurrence_edges);
	free(graph->occurrence_match_cases);
	free(graph->occurrence_fold_clauses);
	free(graph->verification_obligations);
	free(graph->term_relocation);
	free(graph->binding_relocation);
	free(graph->context_relocation);
	free(graph->type_relocation);
	free(graph->type_expr_relocation);
	free(graph->parameter_relocation);
	free(graph->constructor_relocation);
	free(graph->field_relocation);
	free(graph->proposition_relocation);
	free(graph->claim_relocation);
	free(graph->substitution_relocation);
	free(graph->dimension_operator_relocation);
	free(graph->substitution_evidence_claims);
	memset(graph, 0, sizeof(*graph));
}

static int artifact_alloc_bytes(void** p_ret, size_t count, size_t size) {
	if (!p_ret || size == 0) {
		return -1;
	}
	*p_ret = NULL;
	if (count == 0) {
		return 0;
	}
	*p_ret = calloc(count, size);
	return *p_ret ? 0 : -1;
}

static void artifact_init_closure_defaults(struct artifact_closure_graph* graph) {
	for (size_t i = 0; i < graph->judgement.claim_count; ++i) {
		graph->judgement.claims[i].proposition_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.case_count; ++i) {
		graph->terms.cases[i].constructor_owner = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].first_binder = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].body = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.case_binder_count; ++i) {
		graph->terms.case_binders[i].binding_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.ih_scope_count; ++i) {
		graph->terms.ih_scopes[i].match_term = PROTOTYPE_INVALID_ID;
		graph->terms.ih_scopes[i].scrutinee_binding_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.semantic_schema.type_count; ++i) {
		graph->type_declarations.semantic_schema.type_declarations[i].name_symbol_id = -1;
		graph->type_declarations.semantic_schema.type_declarations[i].type_index = PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.type_declarations[i].representation_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.type_declarations[i].parameter_context = PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.type_declarations[i].index_context = PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.type_declarations[i].first_parameter = PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.type_declarations[i].first_constructor = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.readback.parameter_count; ++i) {
		graph->type_declarations.readback.parameter_declarations[i].binding_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.readback.parameter_declarations[i].name_symbol_id = -1;
		graph->type_declarations.readback.parameter_declarations[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.semantic_schema.constructor_count; ++i) {
		graph->type_declarations.semantic_schema.constructor_declarations[i].name_symbol_id = -1;
		graph->type_declarations.semantic_schema.constructor_declarations[i].owner_type = PROTOTYPE_INVALID_ID;
		graph->type_declarations.readback.constructor_readbacks[i].first_field_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.readback.constructor_readbacks[i].result_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.constructor_declarations[i].parameter_context =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.constructor_declarations[i].field_context =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.semantic_schema.constructor_declarations[i].result_classifier =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_classifier_cache.entries[i].classifier =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_classifier_cache.entries[i].schema_revision = 0;
	}
	for (size_t i = 0; i < graph->type_declarations.readback.field_type_count; ++i) {
		graph->type_declarations.readback.field_types[i] = PROTOTYPE_INVALID_ID;
	}
}

static void artifact_marks_free(struct artifact_graph_marks* marks) {
	if (!marks) {
		return;
	}
	free(marks->terms);
	free(marks->cases);
	free(marks->case_binders);
	free(marks->frames);
	free(marks->types);
	free(marks->parameters);
	free(marks->constructors);
	free(marks->readback_field_types);
	free(marks->type_exprs);
	free(marks->propositions);
	free(marks->claims);
	free(marks->derivations);
	free(marks->substitutions);
	free(marks->term_order);
	free(marks->case_order);
	free(marks->case_binder_order);
	free(marks->frame_order);
	free(marks->type_order);
	free(marks->parameter_order);
	free(marks->constructor_order);
	free(marks->readback_field_type_order);
	free(marks->type_expr_order);
	free(marks->proposition_order);
	free(marks->claim_order);
	free(marks->derivation_order);
	free(marks->substitution_order);
	free(marks->substitution_evidence_claims);
	memset(marks, 0, sizeof(*marks));
}

static int artifact_marks_init(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata
) {
	if (!marks || !terms || !type_declarations || !judgement) {
		return -1;
	}
	memset(marks, 0, sizeof(*marks));
	marks->type_declarations = type_declarations;
	marks->judgement = judgement;
	marks->term_count = terms->term_count;
	marks->case_count = terms->case_count;
	marks->case_binder_count = terms->case_binder_count;
	marks->frame_count = terms->ih_scope_count;
	marks->type_count = type_declarations->semantic_schema.type_count;
	marks->parameter_count = type_declarations->readback.parameter_count;
	marks->constructor_count = type_declarations->semantic_schema.constructor_count;
	marks->readback_field_type_count = type_declarations->readback.field_type_count;
	marks->type_expr_count = type_declarations->readback.expr_count;
	marks->proposition_count = judgement->proposition_count;
	marks->claim_count = judgement->claim_count;
	marks->derivation_count = judgement->derivation_count;
	marks->substitution_count = metadata ?
		metadata->substitutions.substitution_count : 0;
	if (artifact_alloc_bytes((void**)&marks->terms, marks->term_count, sizeof(*marks->terms)) != 0 ||
		artifact_alloc_bytes((void**)&marks->cases, marks->case_count, sizeof(*marks->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->case_binders,
			marks->case_binder_count,
			sizeof(*marks->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&marks->frames, marks->frame_count, sizeof(*marks->frames)) != 0 ||
		artifact_alloc_bytes((void**)&marks->types, marks->type_count, sizeof(*marks->types)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->parameters,
			marks->parameter_count,
			sizeof(*marks->parameters)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->constructors,
			marks->constructor_count,
			sizeof(*marks->constructors)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->readback_field_types,
			marks->readback_field_type_count,
			sizeof(*marks->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->type_exprs,
			marks->type_expr_count,
			sizeof(*marks->type_exprs)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->propositions,
			marks->proposition_count,
			sizeof(*marks->propositions)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->claims,
			marks->claim_count,
			sizeof(*marks->claims)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->derivations,
			marks->derivation_count,
			sizeof(*marks->derivations)
		) != 0 || artifact_alloc_bytes(
		(void**)&marks->substitutions,
			marks->substitution_count,
			sizeof(*marks->substitutions)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->term_order,
			marks->term_count,
			sizeof(*marks->term_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->case_order,
			marks->case_count,
			sizeof(*marks->case_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->case_binder_order,
			marks->case_binder_count,
			sizeof(*marks->case_binder_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->frame_order,
			marks->frame_count,
			sizeof(*marks->frame_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->type_order,
			marks->type_count,
			sizeof(*marks->type_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->parameter_order,
			marks->parameter_count,
			sizeof(*marks->parameter_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->constructor_order,
			marks->constructor_count,
			sizeof(*marks->constructor_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->readback_field_type_order,
			marks->readback_field_type_count,
			sizeof(*marks->readback_field_type_order)
		) != 0 || artifact_alloc_bytes(
		(void**)&marks->type_expr_order,
			marks->type_expr_count,
			sizeof(*marks->type_expr_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->proposition_order,
			marks->proposition_count,
			sizeof(*marks->proposition_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->claim_order,
			marks->claim_count,
			sizeof(*marks->claim_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->derivation_order,
			marks->derivation_count,
			sizeof(*marks->derivation_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->substitution_order,
			marks->substitution_count,
			sizeof(*marks->substitution_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->substitution_evidence_claims,
			marks->substitution_count,
			sizeof(*marks->substitution_evidence_claims)
		) != 0) {
		artifact_marks_free(marks);
		return -1;
	}
	for (size_t i = 0; i < marks->substitution_count; ++i) {
		marks->substitution_evidence_claims[i] = PROTOTYPE_INVALID_ID;
	}
	return 0;
}

static int artifact_mark_roots(
	struct artifact_graph_marks* marks,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata
) {
	if (!marks || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	/* A structural TYPE_FORMER does not select a nominal declaration. Root the
	 * exported TypeViews first so representation traversal cannot promote an
	 * allocation-dependent representative declaration into the artifact. */
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (artifact_mark_type(
				marks,
				terms,
				interface->type_exports[i].local_type_id,
				0
			) != 0) {
			fprintf(stderr, "artifact closure: type root traversal failed export=%zu type=%u\n",
				i, interface->type_exports[i].local_type_id);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (!metadata || export->occurrence >= metadata->typed_occurrences.occurrence_count ||
			metadata->typed_occurrences.occurrences[export->occurrence].classifier != export->classifier) {
			fprintf(stderr,
				"artifact closure: term export metadata mismatch export=%zu occurrence=%u "
				"classifier=%u operation_classifier=%u\n",
				i,
				export->occurrence,
				export->classifier,
				metadata && export->occurrence < metadata->typed_occurrences.occurrence_count ?
					metadata->typed_occurrences.occurrences[export->occurrence].classifier :
					PROTOTYPE_INVALID_ID);
			return -1;
		}
		if (artifact_mark_term(marks, terms, export->local_term, 0) != 0 ||
			artifact_mark_subject_relations(marks, terms, judgement, export->local_term) != 0) {
			fprintf(stderr,
				"artifact closure: term export traversal failed export=%zu term=%u\n",
				i, export->local_term);
			return -1;
		}
		if (export->classifier != PROTOTYPE_INVALID_ID) {
			int claim_status = artifact_mark_exact_relation(
					marks,
					terms,
					judgement,
					export->local_term,
					export->classifier
				);
			int residual_status = claim_status > 0 ?
				artifact_export_has_residual_constraint(
					metadata, terms, export->occurrence
				) : 1;
			int classifier_term_status = artifact_mark_term(
				marks, terms, export->classifier, 0
			);
			int classifier_relation_status = classifier_term_status == 0 ?
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->classifier
				) : -1;
			if (claim_status < 0 || (claim_status > 0 && residual_status <= 0) ||
				classifier_term_status != 0 || classifier_relation_status != 0) {
				fprintf(
					stderr,
					"artifact export has neither accepted claim nor residual obligation "
					"occurrence=%u subject=%u classifier=%u claim=%d residual=%d "
					"classifier-term=%d classifier-relations=%d\n",
					export->occurrence,
					export->local_term,
					export->classifier,
					claim_status,
					residual_status,
					classifier_term_status,
					classifier_relation_status
				);
			return -1;
			}
		} else if (artifact_export_has_residual_constraint(
			metadata, terms, export->occurrence
		) <= 0) {
			fprintf(
				stderr,
				"artifact export has neither classifier Claim nor residual obligation "
				"occurrence=%u subject=%u\n",
				export->occurrence,
				export->local_term
			);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		if (artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->source_type_claim_id,
				0
			) != 0 || artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->identity_family_has_type_claim_id,
				0
			) != 0) {
			fprintf(
				stderr,
				"artifact closure: identity claim traversal failed root=%zu source=%u family=%u\n",
				i,
				root->source_type_claim_id,
				root->identity_family_has_type_claim_id
			);
			return -1;
		}
		if (root->witness_has_type_claim_id != PROTOTYPE_INVALID_ID &&
			 artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->witness_has_type_claim_id,
				0
			 ) != 0) {
			fprintf(
				stderr,
				"artifact closure: identity witness claim traversal failed root=%zu claim=%u\n",
				i,
				root->witness_has_type_claim_id
			);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		if (export->formation_classifier == PROTOTYPE_INVALID_ID ||
			artifact_mark_term(marks, terms, export->formation_classifier, 0) != 0 ||
			artifact_mark_subject_relations(
				marks, terms, judgement, export->formation_classifier
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		if (export->curried_classifier_cache != PROTOTYPE_INVALID_ID &&
			(artifact_mark_term(
					marks, terms, export->curried_classifier_cache, 0
				) != 0 ||
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->curried_classifier_cache
				) != 0)) {
			return -1;
		}
	}
	if (metadata) {
		unsigned char reachable_contexts[PROTOTYPE_CONTEXT_CAPACITY];
		memset(reachable_contexts, 0, sizeof(reachable_contexts));
		reachable_contexts[prototype_context_empty(&metadata->contexts)] = 1;
		for (uint32_t type_id = 0;
			type_id < type_declarations->semantic_schema.type_count;
			++type_id) {
			if (type_id >= marks->type_count || !marks->types[type_id]) {
				continue;
			}
			const struct prototype_type_declaration* type =
				&type_declarations->semantic_schema.type_declarations[type_id];
			if (!artifact_type_present(type)) {
				return -1;
			}
			uint32_t type_context_ids[] = {
				type->parameter_context,
				type->index_context
			};
			for (size_t context_index = 0; context_index < 2; ++context_index) {
				uint32_t context_id = type_context_ids[context_index];
				while (context_id != prototype_context_empty(&metadata->contexts)) {
					const struct prototype_context* context = prototype_context_get(
						&metadata->contexts, context_id
					);
					if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
						return -1;
					}
					reachable_contexts[context_id] = 1;
					context_id = context->parent;
				}
			}
			for (uint32_t j = 0; j < type->constructor_count; ++j) {
				const struct prototype_type_constructor_declaration* constructor =
					&type_declarations->semantic_schema.constructor_declarations[
						type->first_constructor + j
					];
				uint32_t context_ids[] = {
					constructor->parameter_context,
					constructor->field_context
				};
				for (size_t k = 0; k < 2; ++k) {
					uint32_t context_id = context_ids[k];
					while (context_id != prototype_context_empty(
						&metadata->contexts
					)) {
						const struct prototype_context* context =
							prototype_context_get(
								&metadata->contexts, context_id
							);
						if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
							fprintf(stderr,
								"artifact closure: constructor Context traversal failed "
								"type=%u constructor=%u context=%u\n",
								type_id, j, context_id);
							return -1;
						}
						reachable_contexts[context_id] = 1;
						context_id = context->parent;
					}
				}
			}
		}
		for (size_t position = 0;
			position < marks->ordered_derivation_count;
			++position) {
			uint32_t derivation_id = marks->derivation_order[position];
			if (derivation_id >= marks->derivation_count ||
				!marks->derivations[derivation_id]) {
				fprintf(stderr,
					"artifact closure: invalid ordered derivation position=%zu derivation=%u\n",
					position, derivation_id);
				return -1;
			}
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[derivation_id];
			if (derivation->semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				artifact_mark_substitution(
					marks,
					terms,
					&metadata->substitutions,
					derivation->semantic_action_id,
					0
				) != 0) {
				fprintf(stderr,
					"artifact closure: derivation substitution traversal failed "
					"derivation=%u substitution=%u\n",
					derivation_id, derivation->semantic_action_id);
				return -1;
			}
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				if (derivation->premises[j].semantic_action_kind ==
						PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
					artifact_mark_substitution(
						marks,
						terms,
						&metadata->substitutions,
						derivation->premises[j].semantic_action_id,
						0
					) != 0) {
					fprintf(stderr,
						"artifact closure: premise substitution traversal failed "
						"derivation=%u premise=%u substitution=%u\n",
						derivation_id,
						j,
						derivation->premises[j].semantic_action_id);
					return -1;
				}
			}
		}
		for (size_t i = 0; i < metadata->typed_occurrences.case_count; ++i) {
			const struct prototype_typed_occurrence_match_case* operation_case =
				&metadata->typed_occurrences.cases[i];
			/* The typed case owns this semantic reference independently of any
			 * refinement action. Constant motives deliberately have no
			 * substitution from which the owner could be reached incidentally. */
			if (artifact_mark_term(
					marks, terms, operation_case->constructor_owner, 0
				) != 0) {
				fprintf(stderr,
					"artifact closure: Match constructor owner traversal failed "
					"case=%zu owner=%u\n",
					i, operation_case->constructor_owner);
				return -1;
			}
			if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
				artifact_mark_substitution(
					marks,
					terms,
					&metadata->substitutions,
					operation_case->refinement_substitution,
					0
				) != 0) {
				fprintf(stderr,
					"artifact closure: Match refinement traversal failed "
					"case=%zu substitution=%u\n",
					i, operation_case->refinement_substitution);
				return -1;
			}
		}
		for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
			const struct prototype_typed_occurrence* operation =
				&metadata->typed_occurrences.occurrences[i];
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID &&
				artifact_mark_substitution(
					marks,
					terms,
					&metadata->substitutions,
					operation->context_action_substitution,
					0
				) != 0) {
				fprintf(stderr,
					"artifact closure: operation Context action traversal failed "
					"occurrence=%zu substitution=%u\n",
					i, operation->context_action_substitution);
				return -1;
			}
		}
		if (artifact_canonicalize_substitution_order(
				marks, &metadata->substitutions
			) != 0) {
			return -1;
		}
		for (size_t i = 0; i < marks->substitution_count; ++i) {
			if (!marks->substitutions[i]) {
				continue;
			}
			const struct prototype_substitution* substitution =
				prototype_substitution_get(
					&metadata->substitutions, (uint32_t)i
				);
			if (!substitution) {
				return -1;
			}
			uint32_t context_ids[] = {
				substitution->source_context,
				substitution->target_context
			};
			for (size_t j = 0; j < 2; ++j) {
				uint32_t context_id = context_ids[j];
				while (context_id != prototype_context_empty(
					&metadata->contexts
				)) {
					const struct prototype_context* context =
						prototype_context_get(&metadata->contexts, context_id);
					if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
						return -1;
					}
					reachable_contexts[context_id] = 1;
					context_id = context->parent;
				}
			}
		}
		for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
			const struct prototype_typed_occurrence* operation = &metadata->typed_occurrences.occurrences[i];
			uint32_t context_id = operation->context_id;
			while (context_id != prototype_context_empty(&metadata->contexts)) {
				const struct prototype_context* context =
					prototype_context_get(&metadata->contexts, context_id);
				if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
					return -1;
				}
				reachable_contexts[context_id] = 1;
				context_id = context->parent;
			}
			uint32_t references[] = {
				operation->core_term,
				operation->classifier,
				operation->binder_classifier,
				operation->source_core_term,
				operation->source_classifier
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
				}
			}
			for (size_t i = 0;
			i < prototype_verification_db_count(&metadata->verification);
			++i) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(&metadata->verification, (uint32_t)i);
			if (!obligation) {
				return -1;
			}
			uint32_t references[] = {
				obligation->core_term,
				obligation->input_classifier,
				obligation->classifier_family,
				obligation->effect_row
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
			}
		}
		for (size_t i = 0; i < marks->claim_count; ++i) {
			if (!marks->claims[i]) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i);
			if (!proposition) {
				return -1;
			}
			uint32_t context_id = proposition->context_id;
			while (context_id != prototype_context_empty(&metadata->contexts)) {
				const struct prototype_context* context =
					prototype_context_get(&metadata->contexts, context_id);
				if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
					return -1;
				}
				reachable_contexts[context_id] = 1;
				context_id = context->parent;
			}
		}
		for (uint32_t i = 1; i < metadata->contexts.context_count; ++i) {
			if (!reachable_contexts[i]) {
				continue;
			}
			const struct prototype_context* context =
				prototype_context_get(&metadata->contexts, i);
			uint32_t classifier = prototype_context_classifier_term(context);
			if (!context ||
				(classifier != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(
						marks, terms, classifier, 0
					) != 0) ||
				(context->producer_computation != PROTOTYPE_INVALID_ID &&
				 artifact_mark_term(
					marks, terms, context->producer_computation, 0
				 ) != 0)) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_closure_graph_alloc(
	struct artifact_closure_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	size_t judgement_capacity = judgement->claim_count;
	if (judgement_capacity < judgement->derivation_count) {
		judgement_capacity = judgement->derivation_count;
	}
	if (judgement_capacity < judgement->proposition_count) {
		judgement_capacity = judgement->proposition_count;
	}
	if (artifact_alloc_bytes((void**)&graph->term_nodes, terms->term_count, sizeof(*graph->term_nodes)) != 0 ||
		artifact_alloc_bytes((void**)&graph->cases, terms->case_count, sizeof(*graph->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_label_symbols,
			terms->case_count,
			sizeof(*graph->case_label_symbols)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_binders,
			terms->case_binder_count,
			sizeof(*graph->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->frames, terms->ih_scope_count, sizeof(*graph->frames)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_nodes,
			type_declarations->semantic_schema.type_count,
			sizeof(*graph->type_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->parameter_declarations,
			type_declarations->readback.parameter_count,
			sizeof(*graph->parameter_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->constructor_declarations,
			type_declarations->semantic_schema.constructor_count,
			sizeof(*graph->constructor_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->constructor_readbacks,
			type_declarations->semantic_schema.constructor_count,
			sizeof(*graph->constructor_readbacks)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->constructor_classifier_cache_entries,
			type_declarations->semantic_schema.constructor_count,
			sizeof(*graph->constructor_classifier_cache_entries)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->readback_field_types,
			type_declarations->readback.field_type_count,
			sizeof(*graph->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_exprs,
			type_declarations->readback.expr_count,
			sizeof(*graph->type_exprs)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_representations,
			type_declarations->semantic_schema.type_count,
			sizeof(*graph->type_representations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->claims, judgement_capacity, sizeof(*graph->claims)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->derivations,
			judgement_capacity,
			sizeof(*graph->derivations)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->propositions, judgement_capacity, sizeof(*graph->propositions)) != 0 ||
		artifact_alloc_bytes((void**)&graph->derivation_candidates, judgement_capacity, sizeof(*graph->derivation_candidates)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->candidate_premises,
			judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			sizeof(*graph->candidate_premises)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->accepted_premises,
			judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			sizeof(*graph->accepted_premises)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->proposition_resource_usage,
			judgement->proposition_resource_usage_count,
			sizeof(*graph->proposition_resource_usage)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_nodes,
			universe->node_count,
			sizeof(*graph->universe_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_edges,
			universe->edge_count,
			sizeof(*graph->universe_edges)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_levels,
			universe->level_count,
			sizeof(*graph->universe_levels)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_constraints,
			universe->constraint_count,
			sizeof(*graph->universe_constraints)
		) != 0) {
		artifact_closure_graph_free(graph);
		return -1;
	}
	prototype_term_db_init(
		&graph->terms,
		graph->term_nodes,
		terms->term_count,
		graph->cases,
		graph->case_label_symbols,
		terms->case_count,
		graph->case_binders,
		terms->case_binder_count,
		graph->frames,
		terms->ih_scope_count
	);
	prototype_type_declaration_db_init(
		&graph->type_declarations,
		graph->type_nodes,
		type_declarations->semantic_schema.type_count,
		graph->constructor_declarations,
		type_declarations->semantic_schema.constructor_count,
		graph->parameter_declarations,
		type_declarations->readback.parameter_count,
		graph->constructor_readbacks,
		type_declarations->semantic_schema.constructor_count,
		graph->readback_field_types,
		type_declarations->readback.field_type_count,
		graph->type_exprs,
		type_declarations->readback.expr_count,
		graph->type_representations,
		type_declarations->semantic_schema.type_count,
		graph->constructor_classifier_cache_entries,
		type_declarations->semantic_schema.constructor_count
	);
	prototype_judgement_db_init(
		&graph->judgement,
		graph->propositions,
		graph->derivation_candidates,
		graph->claims,
		graph->derivations,
		judgement_capacity,
		graph->candidate_premises,
		judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		graph->accepted_premises,
		judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_judgement_db_set_resource_usage_storage(
		&graph->judgement,
		graph->proposition_resource_usage,
		judgement->proposition_resource_usage_count
	);
	prototype_universe_db_init(
		&graph->universe,
		graph->universe_nodes,
		universe->node_count,
		graph->universe_edges,
		universe->edge_count,
		graph->universe_levels,
		universe->level_count,
		graph->universe_constraints,
		universe->constraint_count
	);
	graph->terms.term_count = terms->term_count;
	graph->terms.case_count = terms->case_count;
	graph->terms.case_binder_count = terms->case_binder_count;
	graph->terms.ih_scope_count = terms->ih_scope_count;
	graph->terms.computation_fold_clause_count = terms->computation_fold_clause_count;
	graph->terms.next_binding_id = terms->next_binding_id;
	graph->type_declarations.semantic_schema.type_count = type_declarations->semantic_schema.type_count;
	graph->type_declarations.readback.parameter_count = type_declarations->readback.parameter_count;
	graph->type_declarations.semantic_schema.constructor_count = type_declarations->semantic_schema.constructor_count;
	graph->type_declarations.readback.field_type_count = type_declarations->readback.field_type_count;
	graph->type_declarations.readback.expr_count = type_declarations->readback.expr_count;
	graph->type_declarations.readback.next_level_var = type_declarations->readback.next_level_var;
	graph->judgement.claim_count = judgement->claim_count;
	graph->judgement.derivation_count = judgement->derivation_count;
	graph->judgement.next_universe_var = judgement->next_universe_var;
	graph->universe.node_count = universe->node_count;
	graph->universe.edge_count = universe->edge_count;
	graph->universe.level_count = universe->level_count;
	graph->universe.constraint_count = 0;
	graph->universe.solved = universe->solved;
	artifact_init_closure_defaults(graph);
	return 0;
}

static int artifact_closure_graph_copy_marked(
	struct artifact_closure_graph* graph,
	const struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !marks || !terms || !judgement || !universe) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (marks->terms[i]) {
			graph->terms.terms[i] = terms->terms[i];
		}
	}
	memcpy(
		graph->terms.computation_fold_clauses,
		terms->computation_fold_clauses,
		terms->computation_fold_clause_count * sizeof(*terms->computation_fold_clauses)
	);
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (marks->cases[i]) {
			graph->terms.cases[i] = terms->cases[i];
			graph->terms.case_label_symbols[i] = terms->case_label_symbols[i];
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (marks->case_binders[i]) {
			graph->terms.case_binders[i] = terms->case_binders[i];
		}
	}
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		if (marks->frames[i]) {
			graph->terms.ih_scopes[i] = terms->ih_scopes[i];
		}
	}
	const struct prototype_type_declaration_db* type_declarations =
		marks->type_declarations;
	for (size_t i = 0; i < type_declarations->semantic_schema.type_count; ++i) {
		if (marks->types[i]) {
			graph->type_declarations.semantic_schema.type_declarations[i] =
				type_declarations->semantic_schema.type_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->readback.parameter_count; ++i) {
		if (marks->parameters[i]) {
			graph->type_declarations.readback.parameter_declarations[i] =
				type_declarations->readback.parameter_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->semantic_schema.constructor_count; ++i) {
		if (marks->constructors[i]) {
			graph->type_declarations.semantic_schema.constructor_declarations[i] =
				type_declarations->semantic_schema.constructor_declarations[i];
			graph->type_declarations.readback.constructor_readbacks[i] =
				type_declarations->readback.constructor_readbacks[i];
			graph->type_declarations.constructor_classifier_cache.entries[i] =
				type_declarations->constructor_classifier_cache.entries[i];
		}
	}
	for (size_t i = 0; i < type_declarations->readback.field_type_count; ++i) {
		if (marks->readback_field_types[i]) {
			graph->type_declarations.readback.field_types[i] =
				type_declarations->readback.field_types[i];
		}
	}
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (marks->type_exprs[i]) {
			graph->type_declarations.readback.exprs[i] = type_declarations->readback.exprs[i];
		}
	}
	if (graph->type_declarations.representation_db.representation_capacity <
		type_declarations->representation_db.representation_count) {
		return -1;
	}
	memcpy(
		graph->type_declarations.representation_db.representations,
		type_declarations->representation_db.representations,
		type_declarations->representation_db.representation_count * sizeof(*type_declarations->representation_db.representations)
	);
	graph->type_declarations.representation_db.representation_count = type_declarations->representation_db.representation_count;
	graph->type_declarations.representation_db.cache_dirty = type_declarations->representation_db.cache_dirty;
	graph->judgement.proposition_count = judgement->proposition_count;
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		if (marks->claims[i]) {
			const struct prototype_judgement_claim* source_claim =
				&judgement->claims[i];
			const struct prototype_judgement_proposition* source_proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i);
			if (!source_proposition || source_claim->proposition_id >=
				graph->judgement.proposition_capacity) {
				return -1;
			}
			graph->judgement.propositions[source_claim->proposition_id] =
				*source_proposition;
			graph->judgement.claims[i] = judgement->claims[i];
		}
	}
	graph->judgement.accepted_premise_count = 0;
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		if (marks->derivations[i]) {
			const struct prototype_judgement_derivation* source_derivation =
				&judgement->derivations[i];
			struct prototype_judgement_derivation* target_derivation =
				&graph->judgement.derivations[i];
			if (source_derivation->premise_count >
				graph->judgement.accepted_premise_capacity -
				graph->judgement.accepted_premise_count) {
				return -1;
			}
			*target_derivation = *source_derivation;
			target_derivation->premises = source_derivation->premise_count == 0 ?
				NULL : &graph->judgement.accepted_premises[
					graph->judgement.accepted_premise_count
				];
			for (uint32_t j = 0; j < source_derivation->premise_count; ++j) {
				target_derivation->premises[j] = source_derivation->premises[j];
				if (source_derivation->premises[j].claim_id == PROTOTYPE_INVALID_ID) {
					const struct prototype_judgement_proposition* scoped =
						prototype_judgement_premise_proposition(
							judgement, &source_derivation->premises[j]
						);
					uint32_t proposition_id =
						source_derivation->premises[j].scoped_proposition_id;
					if (!scoped || proposition_id >=
						graph->judgement.proposition_capacity) {
						return -1;
					}
					graph->judgement.propositions[proposition_id] = *scoped;
				}
			}
			graph->judgement.accepted_premise_count +=
				source_derivation->premise_count;
		}
	}
	for (size_t i = 0; i < graph->judgement.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&graph->judgement.propositions[i];
		if (!artifact_candidate_claim_present(proposition) ||
			proposition->resource_usage_count == 0) {
			continue;
		}
		if (!proposition->resource_usage ||
			proposition->resource_usage_count >
				graph->judgement.proposition_resource_usage_capacity -
				graph->judgement.proposition_resource_usage_count) {
			return -1;
		}
		struct prototype_usage_entry* copied =
			&graph->judgement.proposition_resource_usage[
				graph->judgement.proposition_resource_usage_count
			];
		memcpy(
			copied,
			proposition->resource_usage,
			proposition->resource_usage_count * sizeof(*copied)
		);
		proposition->resource_usage = copied;
		graph->judgement.proposition_resource_usage_count +=
			proposition->resource_usage_count;
	}
	if (prototype_judgement_db_rebuild_index(&graph->judgement) != 0) {
		return -1;
	}
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (node->tag == PROTOTYPE_UNIVERSE_NODE_TYPE &&
			node->type_id < marks->type_count &&
			marks->types[node->type_id]) {
			graph->universe.nodes[i] = *node;
		} else if (node->tag == PROTOTYPE_UNIVERSE_NODE_PARAMETER &&
			node->parameter_id < marks->parameter_count &&
			marks->parameters[node->parameter_id]) {
			graph->universe.nodes[i] = *node;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (edge->from_node < graph->universe.node_count &&
			edge->to_node < graph->universe.node_count &&
			artifact_universe_node_present(&graph->universe.nodes[edge->from_node]) &&
			artifact_universe_node_present(&graph->universe.nodes[edge->to_node])) {
			graph->universe.edges[i] = *edge;
		}
	}
	for (size_t i = 0; i < universe->level_count; ++i) {
		graph->universe.levels[i] = universe->levels[i];
	}
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		if (constraint->subject < marks->term_count &&
			constraint->classifier < marks->term_count &&
			marks->terms[constraint->subject] &&
			marks->terms[constraint->classifier] &&
			(constraint->source_claim_id == PROTOTYPE_INVALID_ID ||
			 (constraint->source_claim_id < marks->claim_count &&
			  marks->claims[constraint->source_claim_id]))) {
			graph->universe.constraints[graph->universe.constraint_count++] = *constraint;
		}
	}
	graph->universe.solved = universe->solved;
	return 0;
}

static int artifact_build_closure_graph(
	struct artifact_closure_graph* graph,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const uint32_t* substitution_evidence_claims,
	size_t substitution_evidence_count,
	const struct prototype_universe_db* universe,
	const struct prototype_compile_metadata* metadata
) {
	if (!graph || !interface || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	struct artifact_graph_marks marks;
	if (artifact_marks_init(
			&marks, terms, type_declarations, judgement, metadata
		) != 0) {
		fprintf(stderr, "artifact closure: mark allocation failed\n");
		return -1;
	}
	marks.cwf_certificates = cwf_certificates;
	marks.source_substitution_evidence_claims = substitution_evidence_claims;
	marks.source_substitution_evidence_count = substitution_evidence_count;
	int status = 0;
	if (artifact_mark_roots(
			&marks, interface, terms, type_declarations, judgement, metadata
		) != 0) {
		fprintf(stderr, "artifact closure: root traversal failed\n");
		status = -1;
	} else if (artifact_closure_graph_alloc(
			graph, terms, type_declarations, judgement, universe
		) != 0) {
		fprintf(stderr, "artifact closure: graph allocation failed\n");
		status = -1;
	} else if (artifact_closure_graph_copy_marked(
			graph, &marks, terms, judgement, universe
		) != 0) {
		fprintf(stderr, "artifact closure: marked graph copy failed\n");
		status = -1;
	} else {
		artifact_closure_graph_take_publication_order(graph, &marks);
	}
	artifact_marks_free(&marks);
	if (status != 0) {
		artifact_closure_graph_free(graph);
	}
	return status;
}

static void artifact_reset_object_graph_for_publication(
	struct artifact_closure_graph* graph
) {
	graph->terms.term_count = 0;
	graph->terms.case_count = 0;
	graph->terms.case_binder_count = 0;
	graph->terms.ih_scope_count = 0;
	graph->terms.computation_fold_clause_count = 0;
	graph->terms.next_binding_id = 0;
	graph->type_declarations.semantic_schema.type_count = 0;
	graph->type_declarations.readback.parameter_count = 0;
	graph->type_declarations.semantic_schema.constructor_count = 0;
	graph->type_declarations.readback.field_type_count = 0;
	graph->type_declarations.readback.expr_count = 0;
	graph->type_declarations.representation_db.representation_count = 0;
	graph->type_declarations.representation_db.cache_dirty = 1;
	graph->type_declarations.readback.next_level_var = 0;
	graph->judgement.proposition_count = 0;
	graph->judgement.derivation_candidate_count = 0;
	graph->judgement.claim_count = 0;
	graph->judgement.derivation_count = 0;
	graph->judgement.candidate_premise_count = 0;
	graph->judgement.accepted_premise_count = 0;
	graph->judgement.proposition_resource_usage_count = 0;
	graph->judgement.next_universe_var = 0;
	graph->universe.node_count = 0;
	graph->universe.edge_count = 0;
	graph->universe.level_count = 0;
	graph->universe.constraint_count = 0;
	graph->universe.solved = 0;
}

static int artifact_publication_metadata_relocate(
	struct artifact_publication_graph* publication,
	const struct prototype_compile_metadata* source,
	const struct artifact_closure_graph* closure
) {
	if (!publication || !source || !closure) {
		return -1;
	}
	struct prototype_compile_metadata* target = &publication->metadata;
	target->compile_policy = source->compile_policy;
	target->definition_thunk_policy = source->definition_thunk_policy;
	target->selected_entry_symbol_id = source->selected_entry_symbol_id;
	target->selected_entry_term = artifact_relocate_optional_id(
		source->selected_entry_term,
		publication->term_relocation,
		closure->terms.term_count
	);
	target->selected_entry_classifier = artifact_relocate_optional_id(
		source->selected_entry_classifier,
		publication->term_relocation,
		closure->terms.term_count
	);
	target->selected_entry_occurrence = source->selected_entry_occurrence;
	target->required_runtime_capabilities = source->required_runtime_capabilities;
	target->typed_occurrences.occurrence_count = source->typed_occurrences.occurrence_count;
	for (size_t i = 0; i < source->typed_occurrences.occurrence_count; ++i) {
		target->typed_occurrences.occurrences[i] = source->typed_occurrences.occurrences[i];
		struct prototype_typed_occurrence* operation = &target->typed_occurrences.occurrences[i];
		uint32_t* term_fields[] = {
			&operation->core_term,
			&operation->classifier,
			&operation->binder_classifier,
			&operation->source_core_term,
			&operation->source_classifier
		};
		for (size_t j = 0; j < sizeof(term_fields) / sizeof(term_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*term_fields[j],
				publication->term_relocation,
				closure->terms.term_count
			);
			if (*term_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence term relocation failed occurrence=%zu field=%zu term=%u\n",
					i, j, *term_fields[j]);
				return -1;
			}
			*term_fields[j] = relocated;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			if (operation->core_term >= publication->object.terms.term_count ||
				publication->object.terms.terms[operation->core_term].tag !=
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				return -1;
			}
			operation->ih_scope_id = publication->object.terms.terms[
				operation->core_term
			].as.induction_hypothesis.ih_scope_id;
		}
		if (operation->context_id >= source->contexts.context_count ||
			publication->context_relocation[operation->context_id] ==
				PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"artifact occurrence context relocation failed occurrence=%zu context=%u/%zu\n",
				i, operation->context_id, source->contexts.context_count);
			return -1;
		}
		operation->context_id = publication->context_relocation[
			operation->context_id
		];
		if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
			if (operation->context_action_substitution >=
					source->substitutions.substitution_count ||
				publication->substitution_relocation[
					operation->context_action_substitution
				] == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence action relocation failed occurrence=%zu substitution=%u/%zu\n",
					i, operation->context_action_substitution,
					source->substitutions.substitution_count);
				return -1;
			}
			operation->context_action_substitution =
				publication->substitution_relocation[
					operation->context_action_substitution
				];
		} else if (operation->source_core_term != PROTOTYPE_INVALID_ID ||
			operation->source_classifier != PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"artifact occurrence source pair lacks action occurrence=%zu core=%u classifier=%u\n",
				i, operation->source_core_term, operation->source_classifier);
			return -1;
		}
		uint32_t* binding_fields[] = {
			&operation->binding_id,
			&operation->fold_return_binder_id
		};
		for (size_t j = 0; j < sizeof(binding_fields) /
				sizeof(binding_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*binding_fields[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (*binding_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence binding relocation failed occurrence=%zu field=%zu binding=%u\n",
					i, j, *binding_fields[j]);
				return -1;
			}
			*binding_fields[j] = relocated;
		}
		for (uint32_t j = 0; j < operation->implicit_effect_row_count; ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				operation->implicit_effect_row_binders[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence implicit effect binder relocation failed occurrence=%zu binder=%u binding=%u\n",
					i, j, operation->implicit_effect_row_binders[j]);
				return -1;
			}
			operation->implicit_effect_row_binders[j] = relocated;
		}
	}
	target->typed_occurrences.edge_count = source->typed_occurrences.edge_count;
	for (size_t i = 0; i < source->typed_occurrences.edge_count; ++i) {
		target->typed_occurrences.edges[i] = source->typed_occurrences.edges[i];
		if (target->typed_occurrences.edges[i].child_occurrence >=
			source->typed_occurrences.occurrence_count) {
			fprintf(stderr,
				"artifact occurrence edge relocation failed edge=%zu child=%u/%zu\n",
				i, target->typed_occurrences.edges[i].child_occurrence,
				source->typed_occurrences.occurrence_count);
			return -1;
		}
	}
	target->typed_occurrences.case_count = source->typed_occurrences.case_count;
	for (size_t i = 0; i < source->typed_occurrences.case_count; ++i) {
		target->typed_occurrences.cases[i] = source->typed_occurrences.cases[i];
		struct prototype_typed_occurrence_match_case* operation_case =
			&target->typed_occurrences.cases[i];
		if (operation_case->context_id >= source->contexts.context_count ||
			operation_case->constructor_owner >= closure->terms.term_count ||
			publication->context_relocation[operation_case->context_id] ==
				PROTOTYPE_INVALID_ID ||
			publication->term_relocation[operation_case->constructor_owner] ==
				PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"artifact occurrence case relocation failed case=%zu context=%u/%zu "
				"owner=%u/%zu relocated-context=%u relocated-owner=%u\n",
				i, operation_case->context_id, source->contexts.context_count,
				operation_case->constructor_owner, closure->terms.term_count,
				operation_case->context_id < source->contexts.context_count ?
					publication->context_relocation[operation_case->context_id] :
					PROTOTYPE_INVALID_ID,
				operation_case->constructor_owner < closure->terms.term_count ?
					publication->term_relocation[operation_case->constructor_owner] :
					PROTOTYPE_INVALID_ID);
			return -1;
		}
		operation_case->context_id = publication->context_relocation[
			operation_case->context_id
		];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			if (operation_case->refinement_substitution >=
					source->substitutions.substitution_count ||
				publication->substitution_relocation[
					operation_case->refinement_substitution
				] == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence case action relocation failed case=%zu substitution=%u/%zu\n",
					i, operation_case->refinement_substitution,
					source->substitutions.substitution_count);
				return -1;
			}
			operation_case->refinement_substitution =
				publication->substitution_relocation[
					operation_case->refinement_substitution
				];
		} else {
			operation_case->refinement_substitution = PROTOTYPE_INVALID_ID;
		}
		operation_case->constructor_owner = publication->term_relocation[
			operation_case->constructor_owner
		];
		for (uint32_t j = 0; j < operation_case->binder_count; ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				operation_case->binder_ids[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact occurrence case binder relocation failed case=%zu binder=%u binding=%u\n",
					i, j, operation_case->binder_ids[j]);
				return -1;
			}
			operation_case->binder_ids[j] = relocated;
		}
	}
	target->typed_occurrences.fold_clause_count = source->typed_occurrences.fold_clause_count;
	for (size_t i = 0; i < source->typed_occurrences.fold_clause_count; ++i) {
		target->typed_occurrences.fold_clauses[i] = source->typed_occurrences.fold_clauses[i];
		if (target->typed_occurrences.fold_clauses[i].context_id >=
			source->contexts.context_count) {
			fprintf(stderr,
				"artifact fold clause context relocation failed clause=%zu context=%u/%zu\n",
				i, target->typed_occurrences.fold_clauses[i].context_id,
				source->contexts.context_count);
			return -1;
		}
		target->typed_occurrences.fold_clauses[i].context_id =
			publication->context_relocation[
				target->typed_occurrences.fold_clauses[i].context_id
			];
		uint32_t* binding_fields[] = {
			&target->typed_occurrences.fold_clauses[i].argument_binder_id,
			&target->typed_occurrences.fold_clauses[i].continuation_binder_id
		};
		for (size_t j = 0; j < sizeof(binding_fields) /
				sizeof(binding_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*binding_fields[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (*binding_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact fold clause binder relocation failed clause=%zu field=%zu binding=%u\n",
					i, j, *binding_fields[j]);
				return -1;
			}
			*binding_fields[j] = relocated;
		}
	}
	target->verification.obligation_count = source->verification.obligation_count;
	for (size_t i = 0; i < source->verification.obligation_count; ++i) {
		target->verification.obligations[i] = source->verification.obligations[i];
		struct prototype_verification_obligation* obligation =
			&target->verification.obligations[i];
		uint32_t* term_fields[] = {
			&obligation->core_term,
			&obligation->input_classifier,
			&obligation->classifier_family,
			&obligation->effect_row
		};
		for (size_t j = 0; j < sizeof(term_fields) / sizeof(term_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*term_fields[j],
				publication->term_relocation,
				closure->terms.term_count
			);
			if (*term_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				fprintf(stderr,
					"artifact verification term relocation failed obligation=%zu field=%zu term=%u\n",
					i, j, *term_fields[j]);
				return -1;
			}
			*term_fields[j] = relocated;
		}
		uint32_t relocated_binding = artifact_relocate_optional_id(
			obligation->continuation_binder_id,
			publication->binding_relocation,
			closure->terms.next_binding_id
		);
		if (obligation->continuation_binder_id != PROTOTYPE_INVALID_ID &&
			relocated_binding == PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"artifact verification binder relocation failed obligation=%zu binding=%u\n",
				i, obligation->continuation_binder_id);
			return -1;
		}
		obligation->continuation_binder_id = relocated_binding;
	}
	return 0;
}

static int artifact_publication_graph_alloc(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure,
	const struct prototype_artifact_interface* interface,
	const struct prototype_compile_metadata* metadata
) {
	if (!publication || !closure || !interface || !metadata) {
		return -1;
	}
	if (closure->terms.term_count > SIZE_MAX -
			closure->type_declarations.readback.expr_count ||
		closure->terms.term_count + closure->type_declarations.readback.expr_count >
			SIZE_MAX - interface->type_expr_count ||
		closure->terms.term_count + closure->type_declarations.readback.expr_count +
			interface->type_expr_count >
			SIZE_MAX - closure->type_declarations.semantic_schema.type_count ||
		closure->terms.term_count + closure->type_declarations.readback.expr_count +
			interface->type_expr_count + closure->type_declarations.semantic_schema.type_count >
			SIZE_MAX - interface->dependency_count) {
		return -1;
	}
	size_t dependency_capacity = interface->dependency_count +
		closure->terms.term_count + closure->type_declarations.readback.expr_count +
		interface->type_expr_count + closure->type_declarations.semantic_schema.type_count;
	memset(publication, 0, sizeof(*publication));
#define ALLOC_PUBLICATION(field, count) \
	do { \
		if (artifact_alloc_bytes( \
				(void**)&publication->field, \
				(count), \
				sizeof(*publication->field) \
			) != 0) { \
			artifact_publication_graph_free(publication); \
			return -1; \
		} \
	} while (0)
	ALLOC_PUBLICATION(term_exports, interface->term_export_count);
	ALLOC_PUBLICATION(type_exports, interface->type_export_count);
	ALLOC_PUBLICATION(type_parameters, interface->type_parameter_count);
	ALLOC_PUBLICATION(constructor_exports, interface->constructor_export_count);
	ALLOC_PUBLICATION(
		constructor_field_type_exprs,
		interface->constructor_field_type_expr_count
	);
	ALLOC_PUBLICATION(interface_type_exprs, interface->type_expr_count);
	ALLOC_PUBLICATION(identity_roots, interface->identity_root_count);
	ALLOC_PUBLICATION(dependencies, dependency_capacity);
	ALLOC_PUBLICATION(contexts, metadata->contexts.context_count);
	ALLOC_PUBLICATION(substitutions, metadata->substitutions.substitution_count);
	ALLOC_PUBLICATION(
		dimension_operators, metadata->dimension_operators.operator_count
	);
	ALLOC_PUBLICATION(dimension_images, metadata->dimension_operators.image_count);
	ALLOC_PUBLICATION(
		substitution_evidence_claims,
		metadata->substitutions.substitution_count
	);
	ALLOC_PUBLICATION(occurrences, metadata->typed_occurrences.occurrence_count);
	ALLOC_PUBLICATION(occurrence_edges, metadata->typed_occurrences.edge_count);
	ALLOC_PUBLICATION(occurrence_match_cases, metadata->typed_occurrences.case_count);
	ALLOC_PUBLICATION(
		occurrence_fold_clauses, metadata->typed_occurrences.fold_clause_count
	);
	ALLOC_PUBLICATION(
		verification_obligations, metadata->verification.obligation_count
	);
	ALLOC_PUBLICATION(term_relocation, closure->terms.term_count);
	ALLOC_PUBLICATION(binding_relocation, closure->terms.next_binding_id);
	ALLOC_PUBLICATION(context_relocation, metadata->contexts.context_count);
	ALLOC_PUBLICATION(type_relocation, closure->type_declarations.semantic_schema.type_count);
	ALLOC_PUBLICATION(type_expr_relocation, closure->type_declarations.readback.expr_count);
	ALLOC_PUBLICATION(
		parameter_relocation, closure->type_declarations.readback.parameter_count
	);
	ALLOC_PUBLICATION(
		constructor_relocation, closure->type_declarations.semantic_schema.constructor_count
	);
	ALLOC_PUBLICATION(
		field_relocation, closure->type_declarations.readback.field_type_count
	);
	ALLOC_PUBLICATION(
		proposition_relocation, closure->judgement.proposition_count
	);
	ALLOC_PUBLICATION(claim_relocation, closure->judgement.claim_count);
	ALLOC_PUBLICATION(
		substitution_relocation, metadata->substitutions.substitution_count
	);
	ALLOC_PUBLICATION(
		dimension_operator_relocation,
		metadata->dimension_operators.operator_count
	);
#undef ALLOC_PUBLICATION
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		publication->substitution_evidence_claims[i] = PROTOTYPE_INVALID_ID;
	}
	size_t derived_classifier_term_capacity = 0;
	for (size_t i = 0;
		i < closure->type_declarations.semantic_schema.constructor_count;
		++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&closure->type_declarations.semantic_schema.constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_context* parameter_context =
			prototype_context_get(
				&metadata->contexts, constructor->parameter_context
			);
		const struct prototype_context* field_context = prototype_context_get(
			&metadata->contexts, constructor->field_context
		);
		if (!parameter_context || !field_context ||
			field_context->depth < parameter_context->depth) {
			artifact_publication_graph_free(publication);
			return -1;
		}
		size_t parameter_terms = parameter_context->depth;
		size_t field_terms =
			(size_t)(field_context->depth - parameter_context->depth) * 4;
		if (derived_classifier_term_capacity >
				SIZE_MAX - parameter_terms ||
			derived_classifier_term_capacity + parameter_terms >
				SIZE_MAX - field_terms) {
			artifact_publication_graph_free(publication);
			return -1;
		}
		derived_classifier_term_capacity += parameter_terms + field_terms;
	}
	if (closure->terms.term_count >
		SIZE_MAX - derived_classifier_term_capacity) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_term_db publication_term_capacity = closure->terms;
	publication_term_capacity.term_count = closure->terms.term_count +
		derived_classifier_term_capacity;
	if (artifact_closure_graph_alloc(
			&publication->object,
			&publication_term_capacity,
			&closure->type_declarations,
			&closure->judgement,
			&(struct prototype_universe_db){
				.node_count = PROTOTYPE_UNIVERSE_NODE_CAPACITY,
				.edge_count = PROTOTYPE_UNIVERSE_EDGE_CAPACITY,
				.level_count = PROTOTYPE_UNIVERSE_LEVEL_CAPACITY,
				.constraint_count = PROTOTYPE_UNIVERSE_CONSTRAINT_CAPACITY
			}
		) != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	artifact_reset_object_graph_for_publication(&publication->object);
	prototype_artifact_interface_init(
		&publication->interface,
		publication->term_exports,
		interface->term_export_count,
		publication->type_exports,
		interface->type_export_count,
		publication->type_parameters,
		interface->type_parameter_count,
		publication->constructor_exports,
		interface->constructor_export_count,
		publication->constructor_field_type_exprs,
		interface->constructor_field_type_expr_count,
		publication->interface_type_exprs,
		interface->type_expr_count,
		publication->identity_roots,
		interface->identity_root_count,
		publication->dependencies,
		dependency_capacity
	);
	prototype_compile_metadata_init(
		&publication->metadata,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		publication->contexts,
		metadata->contexts.context_count,
		publication->substitutions,
		metadata->substitutions.substitution_count,
		publication->occurrences,
		metadata->typed_occurrences.occurrence_count,
		publication->occurrence_edges,
		metadata->typed_occurrences.edge_count,
		publication->occurrence_match_cases,
		metadata->typed_occurrences.case_count,
		publication->occurrence_fold_clauses,
		metadata->typed_occurrences.fold_clause_count,
		NULL,
		0,
		publication->verification_obligations,
		metadata->verification.obligation_count
	);
	prototype_compile_metadata_set_dimension_storage(
		&publication->metadata,
		publication->dimension_operators,
		metadata->dimension_operators.operator_count,
		publication->dimension_images,
		metadata->dimension_operators.image_count
	);
	return 0;
}

static int artifact_publication_universe_relocate(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure
) {
	if (!publication || !closure) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* occurrences =
		prototype_compile_metadata_typed_occurrences(&publication->metadata);
	if (prototype_universe_collect(
		&publication->object.universe,
		&publication->object.type_declarations,
		&publication->object.terms,
		occurrences,
		&publication->object.judgement
	) != 0) {
		return -1;
	}
	for (size_t i = 0; i < publication->object.universe.node_count; ++i) {
		/* The owning Type/Parameter carries the semantic name. This field is a
		 * process-local diagnostic cache and must not affect artifact bytes. */
		publication->object.universe.nodes[i].symbol_id = -1;
	}
	return 0;
}

static int artifact_build_publication_graph(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure,
	const struct prototype_artifact_interface* interface,
	const struct prototype_compile_metadata* metadata
) {
	if (artifact_publication_graph_alloc(
			publication, closure, interface, metadata
		) != 0) {
		return -1;
	}
	struct prototype_artifact_graph_relocation additional = {
		.binding_ids = publication->binding_relocation,
		.binding_id_capacity = closure->terms.next_binding_id,
		.type_ids = publication->type_relocation,
		.type_id_capacity = closure->type_declarations.semantic_schema.type_count,
		.type_expr_ids = publication->type_expr_relocation,
		.type_expr_id_capacity = closure->type_declarations.readback.expr_count,
		.parameter_ids = publication->parameter_relocation,
		.parameter_id_capacity = closure->type_declarations.readback.parameter_count,
		.constructor_ids = publication->constructor_relocation,
		.constructor_id_capacity = closure->type_declarations.semantic_schema.constructor_count,
		.field_type_ids = publication->field_relocation,
		.field_type_id_capacity = closure->type_declarations.readback.field_type_count,
		.proposition_ids = publication->proposition_relocation,
		.proposition_id_capacity = closure->judgement.proposition_count,
		.claim_ids = publication->claim_relocation,
		.claim_id_capacity = closure->judgement.claim_count,
		.substitution_ids = publication->substitution_relocation,
		.substitution_id_capacity = metadata->substitutions.substitution_count,
		.dimension_operator_ids = publication->dimension_operator_relocation,
		.dimension_operator_id_capacity =
			metadata->dimension_operators.operator_count
	};
	struct artifact_substitution_slice substitution_slice;
	if (artifact_substitution_slice_init(
			&substitution_slice,
			&closure->judgement,
			&metadata->substitutions,
			metadata
		) != 0 || artifact_substitution_slice_canonicalize(
			&substitution_slice,
			closure->substitution_order,
			closure->ordered_substitution_count
		) != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct artifact_context_slice context_slice;
	if (artifact_context_slice_init(
			&context_slice,
			&closure->type_declarations,
			&closure->judgement,
			metadata,
			&substitution_slice
		) != 0) {
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	if (artifact_context_slice_canonicalize(
			&context_slice,
			&closure->type_declarations,
			&closure->judgement,
			metadata,
			closure->type_order,
			closure->ordered_type_count,
			closure->constructor_order,
			closure->ordered_constructor_count,
			closure->claim_order,
			closure->ordered_claim_count,
			closure->derivation_order,
			closure->ordered_derivation_count,
			closure->substitution_order,
			closure->ordered_substitution_count
		) != 0) {
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_context* compact_contexts = calloc(
		context_slice.context_count,
		sizeof(*compact_contexts)
	);
	struct prototype_substitution* compact_substitutions = calloc(
		substitution_slice.substitution_count == 0 ? 1 :
			substitution_slice.substitution_count,
		sizeof(*compact_substitutions)
	);
	struct prototype_type_declaration* compact_types = malloc(
		(closure->type_declarations.semantic_schema.type_count == 0 ? 1 :
		 closure->type_declarations.semantic_schema.type_count) * sizeof(*compact_types)
	);
	struct prototype_type_constructor_declaration* compact_constructors = malloc(
		(closure->type_declarations.semantic_schema.constructor_count == 0 ? 1 :
		 closure->type_declarations.semantic_schema.constructor_count) * sizeof(*compact_constructors)
	);
	struct prototype_type_constructor_readback* compact_constructor_readbacks = malloc(
		(closure->type_declarations.semantic_schema.constructor_count == 0 ? 1 :
		 closure->type_declarations.semantic_schema.constructor_count) * sizeof(*compact_constructor_readbacks)
	);
	struct prototype_constructor_classifier_cache_entry* compact_constructor_caches = malloc(
		(closure->type_declarations.semantic_schema.constructor_count == 0 ? 1 :
		 closure->type_declarations.semantic_schema.constructor_count) * sizeof(*compact_constructor_caches)
	);
	struct prototype_judgement_proposition* compact_propositions = malloc(
		(closure->judgement.proposition_count == 0 ? 1 :
		 closure->judgement.proposition_count) * sizeof(*compact_propositions)
	);
	uint32_t* compact_context_relocation = calloc(
		context_slice.context_count,
		sizeof(*compact_context_relocation)
	);
	if (!compact_contexts || !compact_substitutions || !compact_types ||
		!compact_constructors || !compact_constructor_readbacks ||
		!compact_constructor_caches || !compact_propositions ||
		!compact_context_relocation) {
		free(compact_contexts);
		free(compact_substitutions);
		free(compact_types);
		free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
		free(compact_propositions);
		free(compact_context_relocation);
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_context_db compact_context_db;
	prototype_context_db_init(
		&compact_context_db,
		compact_contexts,
		context_slice.context_count
	);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		if (!context_slice.reachable[i]) {
			continue;
		}
		uint32_t compact_id = context_slice.relocation[i];
		compact_contexts[compact_id] = metadata->contexts.contexts[i];
		if (i == prototype_context_empty(&metadata->contexts)) {
			continue;
		}
		compact_contexts[compact_id].parent = context_slice.relocation[
			metadata->contexts.contexts[i].parent
		];
	}
	compact_context_db.context_count = context_slice.context_count;
	struct prototype_substitution_db compact_substitution_db;
	prototype_substitution_db_init(
		&compact_substitution_db,
		compact_substitutions,
		substitution_slice.substitution_count
	);
	compact_substitution_db.substitution_count =
		substitution_slice.substitution_count;
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		if (!substitution_slice.reachable[i]) {
			continue;
		}
		uint32_t compact_id = substitution_slice.relocation[i];
		compact_substitutions[compact_id] =
			metadata->substitutions.substitutions[i];
		compact_substitutions[compact_id].source_context = context_slice.relocation[
			compact_substitutions[compact_id].source_context
		];
		compact_substitutions[compact_id].target_context = context_slice.relocation[
			compact_substitutions[compact_id].target_context
		];
		if (compact_substitutions[compact_id].kind ==
				PROTOTYPE_SUBSTITUTION_EXTEND) {
			compact_substitutions[compact_id].first = substitution_slice.relocation[
				compact_substitutions[compact_id].first
			];
		} else if (compact_substitutions[compact_id].kind ==
				PROTOTYPE_SUBSTITUTION_COMPOSE) {
			compact_substitutions[compact_id].first = substitution_slice.relocation[
				compact_substitutions[compact_id].first
			];
			compact_substitutions[compact_id].second = substitution_slice.relocation[
				compact_substitutions[compact_id].second
			];
		}
	}
	/* Compaction directly populates semantic records under dense IDs. Rebuild
	 * runtime-only hashes once after every endpoint has been relocated. */
	int context_rebuild_status =
		prototype_context_db_rebuild_runtime_index_after_bulk_load(
			&compact_context_db
		);
	int substitution_rebuild_status = context_rebuild_status == 0 ?
		prototype_substitution_db_rebuild_runtime_index_after_bulk_load(
			&compact_substitution_db
		) : -1;
	int context_validation_status = substitution_rebuild_status == 0 ?
		prototype_context_db_validate(&compact_context_db, &closure->terms) : -1;
	int substitution_validation_status = context_validation_status == 0 ?
		prototype_substitution_db_validate(
			&compact_substitution_db, &compact_context_db, &closure->terms
		) : -1;
	if (context_rebuild_status != 0 || substitution_rebuild_status != 0 ||
		context_validation_status != 0 || substitution_validation_status != 0) {
		fprintf(stderr, "artifact publication substitution slice validation failed\n");
		free(compact_contexts);
		free(compact_substitutions);
		free(compact_types);
		free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
		free(compact_propositions);
		free(compact_context_relocation);
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	memcpy(
		compact_types,
		closure->type_declarations.semantic_schema.type_declarations,
		closure->type_declarations.semantic_schema.type_count * sizeof(*compact_types)
	);
	memcpy(
		compact_constructors,
		closure->type_declarations.semantic_schema.constructor_declarations,
		closure->type_declarations.semantic_schema.constructor_count * sizeof(*compact_constructors)
	);
	memcpy(
		compact_constructor_readbacks,
		closure->type_declarations.readback.constructor_readbacks,
		closure->type_declarations.semantic_schema.constructor_count * sizeof(*compact_constructor_readbacks)
	);
	memcpy(
		compact_constructor_caches,
		closure->type_declarations.constructor_classifier_cache.entries,
		closure->type_declarations.semantic_schema.constructor_count * sizeof(*compact_constructor_caches)
	);
	struct prototype_type_declaration_db compact_type_declarations =
		closure->type_declarations;
	compact_type_declarations.semantic_schema.type_declarations = compact_types;
	compact_type_declarations.semantic_schema.constructor_declarations = compact_constructors;
	compact_type_declarations.readback.constructor_readbacks = compact_constructor_readbacks;
	compact_type_declarations.constructor_classifier_cache.entries = compact_constructor_caches;
	for (size_t i = 0; i < compact_type_declarations.semantic_schema.type_count; ++i) {
		if (!artifact_type_present(&compact_types[i])) {
			continue;
		}
		compact_types[i].parameter_context = context_slice.relocation[
			compact_types[i].parameter_context
		];
		compact_types[i].index_context = context_slice.relocation[
			compact_types[i].index_context
		];
	}
	for (size_t i = 0; i < compact_type_declarations.semantic_schema.constructor_count; ++i) {
		if (!artifact_constructor_present(&compact_constructors[i])) {
			continue;
		}
		if (compact_constructors[i].owner_type >=
				compact_type_declarations.semantic_schema.type_count) {
			free(compact_contexts);
			free(compact_substitutions);
			free(compact_types);
			free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
			free(compact_propositions);
			free(compact_context_relocation);
			artifact_context_slice_free(&context_slice);
			artifact_substitution_slice_free(&substitution_slice);
			artifact_publication_graph_free(publication);
			return -1;
		}
		int incomplete_readback = 0;
		if (compact_constructor_readbacks[i].field_count > 0) {
			uint32_t first = compact_constructor_readbacks[i].first_field_type;
			uint32_t count = compact_constructor_readbacks[i].field_count;
			if (first == PROTOTYPE_INVALID_ID ||
				first > closure->type_declarations.readback.field_type_count ||
				count > closure->type_declarations.readback.field_type_count - first) {
				incomplete_readback = 1;
			} else {
				for (uint32_t j = 0; j < count; ++j) {
					if (closure->type_declarations.readback.field_types[first + j] ==
						PROTOTYPE_INVALID_ID) {
						incomplete_readback = 1;
						break;
					}
				}
			}
		}
		if (incomplete_readback) {
			/* Source readback metadata is optional. The constructor telescope and
			 * result classifier remain the semantic wire authority. */
			compact_constructor_readbacks[i].first_field_type = PROTOTYPE_INVALID_ID;
			compact_constructor_readbacks[i].field_count = 0;
		}
		compact_constructors[i].parameter_context = context_slice.relocation[
			compact_constructors[i].parameter_context
		];
		compact_constructors[i].field_context = context_slice.relocation[
			compact_constructors[i].field_context
		];
	}
	memcpy(
		compact_propositions,
		closure->judgement.propositions,
		closure->judgement.proposition_count * sizeof(*compact_propositions)
	);
	struct prototype_judgement_db compact_judgement = closure->judgement;
	compact_judgement.propositions = compact_propositions;
	for (size_t i = 0; i < compact_judgement.proposition_count; ++i) {
		if (!artifact_candidate_claim_present(&compact_propositions[i])) {
			continue;
		}
		compact_propositions[i].context_id = context_slice.relocation[
			compact_propositions[i].context_id
		];
	}
	for (size_t i = 0; i < compact_judgement.derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&compact_judgement.derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			if (derivation->semantic_action_id >=
					substitution_slice.source_count ||
				substitution_slice.relocation[derivation->semantic_action_id] ==
					PROTOTYPE_INVALID_ID) {
				free(compact_contexts);
				free(compact_substitutions);
				free(compact_types);
				free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
				free(compact_propositions);
				free(compact_context_relocation);
				artifact_context_slice_free(&context_slice);
				artifact_substitution_slice_free(&substitution_slice);
				artifact_publication_graph_free(publication);
				return -1;
			}
			derivation->semantic_action_id = substitution_slice.relocation[
				derivation->semantic_action_id
			];
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind !=
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				continue;
			}
			uint32_t substitution_id =
				derivation->premises[j].semantic_action_id;
			if (substitution_id >= substitution_slice.source_count ||
				substitution_slice.relocation[substitution_id] ==
					PROTOTYPE_INVALID_ID) {
				free(compact_contexts);
				free(compact_substitutions);
				free(compact_types);
				free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
				free(compact_propositions);
				free(compact_context_relocation);
				artifact_context_slice_free(&context_slice);
				artifact_substitution_slice_free(&substitution_slice);
				artifact_publication_graph_free(publication);
				return -1;
			}
			derivation->premises[j].semantic_action_id =
				substitution_slice.relocation[substitution_id];
		}
	}
	const struct artifact_append_order publication_order = {
		.terms = closure->term_order,
		.term_count = closure->ordered_term_count,
		.types = closure->type_order,
		.type_count = closure->ordered_type_count,
		.type_exprs = closure->type_expr_order,
		.type_expr_count = closure->ordered_type_expr_count,
		.fields = closure->readback_field_type_order,
		.field_count = closure->ordered_readback_field_type_count,
		.parameters = closure->parameter_order,
		.parameter_count = closure->ordered_parameter_count,
		.constructors = closure->constructor_order,
		.constructor_count = closure->ordered_constructor_count,
		.propositions = closure->proposition_order,
		.proposition_count = closure->ordered_proposition_count,
		.claims = closure->claim_order,
		.claim_count = closure->ordered_claim_count,
		.derivations = closure->derivation_order,
		.derivation_count = closure->ordered_derivation_count,
		.substitutions = closure->substitution_order,
		.substitution_count = closure->ordered_substitution_count
	};
	if (prototype_internal_artifact_append_graph_ordered(
			&publication->interface,
			&publication->object.terms,
			&publication->object.type_declarations,
			&publication->object.judgement,
			&publication->metadata.contexts,
			&publication->metadata.substitutions,
			&publication->metadata.dimension_operators,
			interface,
			&closure->terms,
			&compact_type_declarations,
			&compact_judgement,
			&compact_context_db,
			&compact_substitution_db,
			&metadata->dimension_operators,
			0,
			publication->term_relocation,
			closure->terms.term_count,
				compact_context_relocation,
				context_slice.context_count,
				&additional,
				0,
				&publication_order
			) != 0) {
		fprintf(stderr, "artifact publication append failed\n");
		free(compact_contexts);
		free(compact_substitutions);
		free(compact_types);
		free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
		free(compact_propositions);
		free(compact_context_relocation);
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		uint32_t source_claim = closure->substitution_evidence_claims[i];
		if (source_claim == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (i >= substitution_slice.source_count ||
			!substitution_slice.reachable[i]) {
			artifact_context_slice_free(&context_slice);
			artifact_substitution_slice_free(&substitution_slice);
			artifact_publication_graph_free(publication);
			return -1;
		}
		uint32_t compact_substitution = substitution_slice.relocation[i];
		if (compact_substitution >= additional.substitution_id_capacity ||
			publication->substitution_relocation[compact_substitution] ==
				PROTOTYPE_INVALID_ID ||
			source_claim >= additional.claim_id_capacity ||
			publication->claim_relocation[source_claim] == PROTOTYPE_INVALID_ID) {
			artifact_context_slice_free(&context_slice);
			artifact_substitution_slice_free(&substitution_slice);
			artifact_publication_graph_free(publication);
			return -1;
		}
		uint32_t published_substitution =
			publication->substitution_relocation[compact_substitution];
		uint32_t published_claim = publication->claim_relocation[source_claim];
		uint32_t selected_claim = publication->substitution_evidence_claims[
			published_substitution
		];
		if (selected_claim == PROTOTYPE_INVALID_ID ||
			published_claim < selected_claim) {
			publication->substitution_evidence_claims[published_substitution] =
				published_claim;
		}
	}
	/* The append relocation is indexed by compact substitution ID. Operation
	 * metadata still refers to source IDs, so compose both relocation stages
	 * before relocating that metadata. */
	uint32_t* source_substitution_relocation = malloc(
		(metadata->substitutions.substitution_count == 0 ? 1 :
		 metadata->substitutions.substitution_count) *
		sizeof(*source_substitution_relocation)
	);
	if (!source_substitution_relocation) {
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		source_substitution_relocation[i] = PROTOTYPE_INVALID_ID;
		if (!substitution_slice.reachable[i]) {
			continue;
		}
		uint32_t compact_substitution = substitution_slice.relocation[i];
		if (compact_substitution >= substitution_slice.substitution_count ||
			publication->substitution_relocation[compact_substitution] ==
				PROTOTYPE_INVALID_ID) {
			free(source_substitution_relocation);
			artifact_context_slice_free(&context_slice);
			artifact_substitution_slice_free(&substitution_slice);
			artifact_publication_graph_free(publication);
			return -1;
		}
		source_substitution_relocation[i] =
			publication->substitution_relocation[compact_substitution];
	}
	memcpy(
		publication->substitution_relocation,
		source_substitution_relocation,
		metadata->substitutions.substitution_count *
			sizeof(*source_substitution_relocation)
	);
	free(source_substitution_relocation);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		publication->context_relocation[i] = context_slice.reachable[i] ?
			compact_context_relocation[context_slice.relocation[i]] :
			PROTOTYPE_INVALID_ID;
	}
	int metadata_status = artifact_publication_metadata_relocate(
		publication, metadata, closure
	);
	struct prototype_typed_occurrence_graph* publication_operations =
		prototype_compile_metadata_typed_occurrences(&publication->metadata);
	int accepted_status = metadata_status == 0 ?
		prototype_judgement_recompute_closure_ranks(
			publication_operations, &publication->object.judgement
		) : -1;
	int universe_status = accepted_status == 0 ?
		artifact_publication_universe_relocate(publication, closure) : -1;
	int key_status = metadata_status == 0 && universe_status == 0 ?
		prototype_artifact_interface_recompute_keys(
			&publication->interface,
			&publication->object.terms,
			&publication->object.type_declarations,
			&publication->metadata.contexts
		) : -1;
	int status = metadata_status == 0 && accepted_status == 0 &&
		universe_status == 0 && key_status == 0 ?
		0 : -1;
	if (status != 0) {
		fprintf(
			stderr,
			"artifact publication relocation failed metadata=%d accepted=%d "
			"universe=%d keys=%d\n",
			metadata_status,
			accepted_status,
			universe_status,
			key_status
		);
	}
	free(compact_contexts);
	free(compact_substitutions);
	free(compact_types);
	free(compact_constructors);
		free(compact_constructor_readbacks);
		free(compact_constructor_caches);
	free(compact_propositions);
	free(compact_context_relocation);
	artifact_context_slice_free(&context_slice);
	artifact_substitution_slice_free(&substitution_slice);
	if (status != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	return 0;
}

static int artifact_republish_publication_graph(
	struct artifact_publication_graph* target,
	const struct artifact_publication_graph* source
) {
	if (!target || !source) {
		return -1;
	}
	struct artifact_closure_graph closure;
	if (artifact_build_closure_graph(
			&closure,
			&source->interface,
			&source->object.terms,
			&source->object.type_declarations,
			&source->object.judgement,
			NULL,
			source->substitution_evidence_claims,
			source->metadata.substitutions.substitution_count,
			&source->object.universe,
			&source->metadata
		) != 0) {
		return -1;
	}
	if (artifact_build_publication_graph(
			target,
			&closure,
			&source->interface,
			&source->metadata
		) != 0) {
		artifact_closure_graph_free(&closure);
		return -1;
	}
	artifact_closure_graph_free(&closure);
	if (prototype_artifact_interface_collect_dependencies(
			&target->interface,
			&target->object.terms,
			&target->object.type_declarations,
			&target->object.judgement
		) != 0) {
		artifact_publication_graph_free(target);
		return -1;
	}
	for (size_t i = 0; i < source->interface.dependency_count; ++i) {
		const struct prototype_artifact_dependency* dependency =
			&source->interface.dependencies[i];
		if (prototype_artifact_interface_add_dependency_in_namespace(
				&target->interface,
				dependency->namespace_symbol_id,
				dependency->name_symbol_id
			) != 0) {
			artifact_publication_graph_free(target);
			return -1;
		}
	}
	return 0;
}

static const char* artifact_optional_symbol_name(
	const struct symbol_table* symbols,
	int symbol_id
) {
	if (symbol_id < 0) {
		return "-";
	}
	return symbol_to_string(symbols, symbol_id);
}
