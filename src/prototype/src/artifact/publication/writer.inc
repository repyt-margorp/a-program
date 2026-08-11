static int write_artifact_operation_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const struct artifact_context_slice* context_slice
) {
	if (!stream || !symbols || !type_declarations || !judgement ||
		!metadata || !context_slice) {
		return -1;
	}
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph_const(metadata, &graph);
	size_t operation_count = prototype_operation_graph_count(&graph);
	size_t case_count = prototype_operation_graph_case_count(&graph);
	size_t fold_clause_count = graph.fold_clause_count;
	size_t obligation_count = metadata ?
		prototype_verification_db_count(&metadata->verification) : 0;
	fprintf(stream, "SECTION operation_graph\n");
	fprintf(
		stream,
		"compile_policy %d %d %s %u %u %u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
		" %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
		metadata ? metadata->compile_policy : PROTOTYPE_COMPILE_POLICY_HYBRID,
		metadata ? metadata->definition_thunk_policy :
			PROTOTYPE_DEFINITION_THUNK_IMPLICIT,
		metadata ? artifact_optional_symbol_name(
			symbols, metadata->selected_entry_symbol_id
		) : "-",
		metadata ? metadata->selected_entry_term : PROTOTYPE_INVALID_ID,
		metadata ? metadata->selected_entry_classifier : PROTOTYPE_INVALID_ID,
		metadata ? metadata->selected_entry_operation : PROTOTYPE_INVALID_ID,
		metadata ? metadata->required_runtime_capabilities : 0,
		metadata ? metadata->normalization_step_limit : 0,
		metadata ? metadata->normalization_steps_used : 0,
		metadata ? metadata->solver_step_limit : 0,
		metadata ? metadata->solver_steps_used : 0,
		metadata ? metadata->solver_exhausted : 0,
		metadata ? metadata->solver_constraint_count : 0,
		metadata ? metadata->solver_solved_count : 0,
		metadata ? metadata->solver_residual_count : 0,
		metadata ? metadata->solver_incomplete_count : 0
	);
	fprintf(stream, "contexts %zu\n", context_slice->context_count);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		if (!context_slice->reachable[i]) {
			continue;
		}
		const struct prototype_context* context =
			&metadata->contexts.contexts[i];
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		if (classifier == PROTOTYPE_INVALID_ID &&
			classifier_variable != PROTOTYPE_INVALID_ID) {
			const struct prototype_operation_node* classifier_operation =
				prototype_operation_graph_get(
					&graph, classifier_variable
				);
			if (!classifier_operation ||
				classifier_operation->classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			classifier = classifier_operation->classifier;
		}
		if (i != 0 && classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		fprintf(
			stream,
			"context %zu %u %u %u %u\n",
			(size_t)context_slice->relocation[i],
			context->parent == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID :
				artifact_context_slice_relocate(
					context_slice, context->parent
				),
			context->binding_id,
			classifier,
			context->depth
		);
	}
	size_t substitution_count = metadata ?
		metadata->substitutions.substitution_count : 0;
	fprintf(stream, "substitutions %zu\n", substitution_count);
	for (size_t i = 0; i < substitution_count; ++i) {
		const struct prototype_substitution* substitution =
			&metadata->substitutions.substitutions[i];
		fprintf(
			stream,
			"substitution %zu %d %u %u %u %u %u %u\n",
			i,
			substitution->kind,
			artifact_context_slice_relocate(
				context_slice, substitution->source_context
			),
			artifact_context_slice_relocate(
				context_slice, substitution->target_context
			),
			substitution->first,
			substitution->second,
			substitution->term,
			substitution->term_classifier
		);
	}
	fprintf(stream, "operations %zu\n", operation_count);
	for (size_t i = 0; i < operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(&graph, (uint32_t)i);
		if (!operation) {
			return -1;
		}
		const char* source_name = artifact_optional_symbol_name(
			symbols, operation->source_symbol_id
		);
		const char* binder_name = artifact_optional_symbol_name(
			symbols, operation->binder_symbol_id
		);
		if (!source_name || !binder_name) {
			return -1;
		}
		fprintf(
			stream,
			"operation %zu %d %d %d %u %u %u %s %s %u %u %u %u %u %u %u"
			" %u %u %u %u %u %u %u %u %u\n",
			i,
			operation->tag,
			operation->category,
			operation->application_role,
			operation->core_term,
			operation->known_classifier,
			operation->classifier,
			source_name,
			binder_name,
			operation->referenced_ast_binder_id,
			operation->binding_id,
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee,
			operation->binder_classifier,
			operation->fold_return_ast_binder_id,
			operation->fold_return_binder_id,
			operation->fold_return_operation,
			operation->implicit_effect_row_count,
			operation->first_case,
			operation->case_count,
			operation->first_fold_clause,
			operation->fold_clause_count,
			artifact_context_slice_relocate(
				context_slice, operation->context_id
			)
		);
		fprintf(stream, "operation_rows %zu", i);
		for (uint32_t j = 0; j < operation->implicit_effect_row_count; ++j) {
			fprintf(stream, " %u", operation->implicit_effect_row_binders[j]);
		}
		fprintf(stream, "\n");
	}
	fprintf(stream, "operation_fold_clauses %zu\n", fold_clause_count);
	for (size_t i = 0; i < fold_clause_count; ++i) {
		const struct prototype_operation_computation_fold_clause* clause =
			prototype_operation_graph_get_fold_clause(&graph, (uint32_t)i);
		if (!clause) {
			return -1;
		}
		fprintf(
			stream,
			"operation_fold_clause %zu %u %u %u %u %u %u %u %u\n",
			i,
			clause->operation_operation,
			clause->body_operation,
			clause->clause_operation,
			artifact_context_slice_relocate(context_slice, clause->context_id),
			clause->argument_ast_binder_id,
			clause->argument_binder_id,
			clause->continuation_ast_binder_id,
			clause->continuation_binder_id
		);
	}
	fprintf(stream, "operation_cases %zu\n", case_count);
	for (size_t i = 0; i < case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(&graph, (uint32_t)i);
		if (!operation_case) {
			return -1;
		}
		const char* label = artifact_optional_symbol_name(
			symbols, operation_case->case_label_symbol_id
		);
		if (!label) {
			return -1;
		}
		fprintf(
			stream,
			"operation_case %zu %u %u %u %u %s\n",
			i,
			operation_case->body_operation,
			artifact_context_slice_relocate(
				context_slice, operation_case->context_id
			),
			operation_case->constructor_owner,
			operation_case->constructor_id,
			label
		);
		fprintf(stream, "operation_case_binders %zu %u", i, operation_case->binder_count);
		for (uint32_t j = 0; j < operation_case->binder_count; ++j) {
			fprintf(stream, " %u", operation_case->ast_binder_ids[j]);
		}
		fprintf(stream, "\n");
	}
	size_t effect_constraint_count = metadata ?
		metadata->effect_constraint_count : 0;
	fprintf(stream, "effect_constraints %zu\n", effect_constraint_count);
	for (size_t i = 0; i < effect_constraint_count; ++i) {
		const struct prototype_operation_effect_constraint* constraint =
			&metadata->effect_constraints[i];
		fprintf(
			stream,
			"effect_constraint %zu %d %d %u %u %u %u\n",
			i,
			constraint->kind,
			constraint->state,
			constraint->operation,
			constraint->result_row,
			constraint->left_row,
			constraint->right_row
		);
	}
	fprintf(stream, "verification_obligations %zu\n", obligation_count);
	for (size_t i = 0; i < obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(&metadata->verification, (uint32_t)i);
		if (!obligation) {
			return -1;
		}
		fprintf(
			stream,
			"verification %zu %d %d %u %u %u %u %u %u %u %u %d %u\n",
			i,
			obligation->kind,
			obligation->state,
			obligation->operation,
			obligation->core_term,
			obligation->computation_operation,
			obligation->continuation_operation,
			obligation->continuation_binder_id,
			obligation->input_classifier,
			obligation->classifier_family,
			obligation->effect_row,
			obligation->normalization_profile,
			obligation->schema_version
		);
	}
	return fprintf(stream, "END operation_graph\n") < 0 ? -1 : 0;
}

static int prototype_artifact_write_text_body(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}
	(void)asts;

	fprintf(
		stream,
		"A_PROGRAM_ARTIFACT %d %s\n",
		PROTOTYPE_ARTIFACT_FORMAT_VERSION,
		PROTOTYPE_ARTIFACT_CALCULUS_FINGERPRINT
	);
	fprintf(stream, "SECTION interface\n");
	size_t present_interface_type_expr_count = 0;
	size_t present_interface_parameter_count = 0;
	size_t present_interface_field_ref_count = 0;
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (artifact_type_expr_present(&interface->type_exprs[i])) {
			present_interface_type_expr_count++;
		}
	}
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		if (artifact_interface_parameter_present(&interface->type_parameters[i])) {
			present_interface_parameter_count++;
		}
	}
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (interface->constructor_field_type_exprs[i] != PROTOTYPE_INVALID_ID) {
			present_interface_field_ref_count++;
		}
	}
	fprintf(
		stream,
		"interface_type_exprs slots %zu type_exprs %zu\n",
		interface->type_expr_count,
		present_interface_type_expr_count
	);
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (!artifact_type_expr_present(&interface->type_exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&interface->type_exprs[i]
			) != 0) {
			return -1;
		}
	}
	fprintf(
		stream,
		"interface_type_parameters slots %zu parameters %zu\n",
		interface->type_parameter_count,
		present_interface_parameter_count
	);
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		const struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[i];
		if (!artifact_interface_parameter_present(parameter)) {
			continue;
		}
		fprintf(
			stream,
			"interface_type_parameter %zu %u %s %u\n",
			i,
			parameter->binding_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}
	fprintf(
		stream,
		"interface_constructor_field_refs slots %zu field_refs %zu\n",
		interface->constructor_field_type_expr_count,
		present_interface_field_ref_count
	);
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (interface->constructor_field_type_exprs[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		fprintf(
			stream,
			"interface_constructor_field_ref %zu %u\n",
			i,
			interface->constructor_field_type_exprs[i]
		);
	}
	fprintf(stream, "term_exports %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		uint32_t source_claim_id = PROTOTYPE_INVALID_ID;
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		if (export->source_evidence.kind ==
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM) {
			if (!prototype_judgement_claim_get(
					judgement, export->source_evidence.id
				)) {
				return -1;
			}
			source_claim_id = export->source_evidence.id;
		} else if (export->source_evidence.kind !=
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID ||
			export->source_evidence.id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		fprintf(
			stream,
			"term %s %u %u %d ",
			name,
			export->local_term,
			export->classifier,
			export->transparency
		);
		print_artifact_key(stream, &export->canonical_key);
		fprintf(stream, " ");
		print_artifact_key(stream, &export->classifier_key);
		fprintf(
			stream,
			" namespace %s operation %u claim %u\n",
			namespace_name,
			export->operation,
			source_claim_id
		);
	}

	fprintf(stream, "type_exports %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"type %s %u %u %u %u %u %u %u ",
			name,
			export->local_type_id,
			export->core_representation_anchor_type_id,
			export->formation_classifier,
			export->first_parameter,
			export->parameter_count,
			export->first_constructor_export,
			export->constructor_count
		);
		print_artifact_type_representation_fingerprint(stream, &export->representation_fingerprint);
		fprintf(stream, " namespace %s\n", namespace_name);
	}

	fprintf(stream, "constructor_exports %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"constructor %u %s %u %u %u %u\n",
			export->type_export_index,
			name,
			export->ordinal,
			export->readback_first_field_type,
			export->readback_field_count,
			export->curried_classifier_cache
		);
	}

	fprintf(stream, "identity_roots %zu\n", interface->identity_root_count);
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		fprintf(
			stream,
			"identity_root %zu %u %u %u %d\n",
			i,
			root->source_type_claim_id,
			root->identity_family_has_type_claim_id,
			root->witness_has_type_claim_id,
			root->computation_rule
		);
	}

	fprintf(stream, "dependencies %zu\n", interface->dependency_count);
	for (size_t i = 0; i < interface->dependency_count; ++i) {
		const struct prototype_artifact_dependency* dependency = &interface->dependencies[i];
		const char* name = symbol_to_string(symbols, dependency->name_symbol_id);
		const char* namespace_name = dependency->namespace_symbol_id < 0 ?
			"-" :
			symbol_to_string(symbols, dependency->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(stream, "dependency %s namespace %s\n", name, namespace_name);
	}
	fprintf(stream, "END interface\n");

	struct artifact_context_slice context_slice;
	if (artifact_context_slice_init(
		&context_slice,
		type_declarations,
		judgement,
		metadata,
		NULL
	) != 0) {
		return -1;
	}
	if (write_artifact_graph_section(
			stream,
			symbols,
			terms,
			type_declarations,
			judgement,
			&context_slice
		) != 0) {
		artifact_context_slice_free(&context_slice);
		return -1;
	}
	if (write_artifact_operation_graph_section(
		stream,
		symbols,
		type_declarations,
		judgement,
		metadata,
		&context_slice
	) != 0) {
		artifact_context_slice_free(&context_slice);
		return -1;
	}
	artifact_context_slice_free(&context_slice);
	if (write_artifact_universe_section(stream, universe) != 0) {
		return -1;
	}
	if (write_artifact_debug_section(stream, symbols, interface, NULL) != 0) {
		return -1;
	}
	return write_artifact_relocation_section(
		stream,
		symbols,
		interface,
		terms,
		type_declarations,
		judgement
	);
}

int prototype_artifact_write_text(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}
	if (metadata &&
		(prototype_context_db_validate(&metadata->contexts, terms) != 0 ||
		 prototype_substitution_db_validate(
			&metadata->substitutions, &metadata->contexts, terms
		 ) != 0 ||
		 prototype_constructor_telescopes_validate(
			type_declarations, &metadata->contexts, terms
		 ) != 0)) {
		fprintf(stderr, "artifact source graph validation failed\n");
		return -1;
	}
	if (interface->identity_root_count != 0 &&
		(!metadata || prototype_artifact_interface_validate_identity_roots(
			interface,
			terms,
			type_declarations,
			&metadata->contexts,
			judgement
		) != 0)) {
		fprintf(stderr, "artifact source identity-root validation failed\n");
		return -1;
	}
	struct artifact_closure_graph closure_graph;
	if (artifact_build_closure_graph(
			&closure_graph,
			interface,
			terms,
			type_declarations,
			judgement,
			universe,
		metadata
	) != 0) {
		fprintf(stderr, "artifact closure construction failed\n");
		return -1;
	}
	struct artifact_publication_graph publication;
	if (artifact_build_publication_graph(
			&publication,
			&closure_graph,
			interface,
		metadata
	) != 0) {
		fprintf(stderr, "artifact dense publication construction failed\n");
		artifact_closure_graph_free(&closure_graph);
		return -1;
	}
	/* Dependencies are part of the public rooted graph. Recompute them after
	 * dense publication so unused accepted work cannot leak imports, while
	 * external references reachable only through proof evidence are retained. */
	if (prototype_artifact_interface_collect_dependencies(
			&publication.interface,
			&publication.object.terms,
			&publication.object.type_declarations,
			&publication.object.judgement
		) != 0) {
		fprintf(stderr, "artifact publication dependency collection failed\n");
		artifact_publication_graph_free(&publication);
		artifact_closure_graph_free(&closure_graph);
		return -1;
	}
	if (asts) {
		for (size_t i = 0; i < publication.object.terms.term_count; ++i) {
			const struct prototype_term* term = &publication.object.terms.terms[i];
			if (term->tag != PROTOTYPE_TERM_TYPE_DECLARATION) {
				continue;
			}
			for (size_t j = 0; j < interface->dependency_count; ++j) {
				const struct prototype_artifact_dependency* dependency =
					&interface->dependencies[j];
				if (dependency->namespace_symbol_id !=
						term->as.type_declaration.identity.namespace_symbol_id ||
					dependency->name_symbol_id !=
						term->as.type_declaration.identity.name_symbol_id) {
					continue;
				}
				if (prototype_artifact_interface_add_dependency_in_namespace(
						&publication.interface,
						dependency->namespace_symbol_id,
						dependency->name_symbol_id
					) != 0) {
					fprintf(stderr, "artifact rooted type dependency collection failed\n");
					artifact_publication_graph_free(&publication);
					artifact_closure_graph_free(&closure_graph);
					return -1;
				}
				break;
			}
		}
		for (size_t i = 0; i < asts->import_count; ++i) {
			if (prototype_artifact_interface_add_dependency(
					&publication.interface, asts->imports[i].name_symbol_id
				) != 0) {
				fprintf(stderr, "artifact source import collection failed\n");
				artifact_publication_graph_free(&publication);
				artifact_closure_graph_free(&closure_graph);
				return -1;
			}
		}
	}
	if (publication.object.terms.term_count != 0 ||
		publication.object.type_declarations.type_count != 0 ||
		publication.object.judgement.claim_count != 0) {
		struct artifact_publication_graph normalized_publication;
		if (artifact_republish_publication_graph(
				&normalized_publication, &publication
			) != 0) {
			fprintf(stderr, "artifact publication normalization failed\n");
			artifact_publication_graph_free(&publication);
			artifact_closure_graph_free(&closure_graph);
			return -1;
		}
		artifact_publication_graph_free(&publication);
		publication = normalized_publication;
	}
	int identity_validation_status =
		prototype_artifact_interface_validate_identity_roots(
			&publication.interface,
			&publication.object.terms,
			&publication.object.type_declarations,
			&publication.metadata.contexts,
			&publication.object.judgement
		);
	if (identity_validation_status != 0) {
		fprintf(
			stderr,
			"artifact dense identity validation failed status=%d\n",
			identity_validation_status
		);
		artifact_publication_graph_free(&publication);
		artifact_closure_graph_free(&closure_graph);
		return -1;
	}
	int status = prototype_artifact_write_text_body(
		stream,
		symbols,
		&publication.interface,
		&publication.object.terms,
		&publication.object.type_declarations,
		&publication.object.judgement,
		&publication.object.universe,
		asts,
		&publication.metadata
	);
	if (status != 0) {
		fprintf(stderr, "artifact dense publication write failed\n");
	}
	artifact_publication_graph_free(&publication);
	artifact_closure_graph_free(&closure_graph);
	return status;
}
