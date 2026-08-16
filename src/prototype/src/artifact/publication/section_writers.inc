static int write_artifact_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct artifact_context_slice* context_slice
) {
	if (!stream || !symbols || !intrinsic_environment || !terms ||
		!type_declarations || !judgement || !context_slice) {
		return -1;
	}
	fprintf(stream, "SECTION graph\n");
	size_t present_term_count = 0;
	size_t present_case_count = 0;
	size_t present_case_binder_count = 0;
	size_t present_frame_count = 0;
	size_t present_type_count = 0;
	size_t present_parameter_count = 0;
	size_t present_constructor_count = 0;
	size_t present_field_type_count = 0;
	size_t present_type_expr_count = 0;
	size_t present_proposition_count = 0;
	size_t present_claim_count = 0;
	size_t present_derivation_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (artifact_term_present(&terms->terms[i])) {
			present_term_count++;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (artifact_case_present(&terms->cases[i])) {
			present_case_count++;
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (artifact_case_binder_present(&terms->case_binders[i])) {
			present_case_binder_count++;
		}
	}
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		if (artifact_frame_present(&terms->ih_scopes[i])) {
			present_frame_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		if (artifact_type_present(&type_declarations->type_declarations[i])) {
			present_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.parameter_count; ++i) {
		if (artifact_parameter_present(&type_declarations->readback.parameter_declarations[i])) {
			present_parameter_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		if (artifact_constructor_present(&type_declarations->constructor_declarations[i])) {
			present_constructor_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.field_type_count; ++i) {
		if (artifact_field_type_present(&type_declarations->readback.field_types[i])) {
			present_field_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (artifact_type_expr_present(&type_declarations->readback.exprs[i])) {
			present_type_expr_count++;
		}
	}
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		if (artifact_claim_present(judgement, &judgement->claims[i])) {
			present_claim_count++;
		}
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		if (artifact_candidate_claim_present(&judgement->propositions[i])) {
			present_proposition_count++;
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		if (artifact_derivation_present(&judgement->derivations[i])) {
			present_derivation_count++;
		}
	}
	if (present_term_count != terms->term_count ||
		present_case_count != terms->case_count ||
		present_case_binder_count != terms->case_binder_count ||
		present_frame_count != terms->ih_scope_count ||
		present_type_count != type_declarations->type_count ||
		present_parameter_count != type_declarations->readback.parameter_count ||
		present_constructor_count != type_declarations->constructor_count ||
		present_field_type_count !=
			type_declarations->readback.field_type_count ||
		present_type_expr_count != type_declarations->readback.expr_count ||
		present_proposition_count != judgement->proposition_count ||
		present_claim_count != judgement->claim_count ||
		present_derivation_count != judgement->derivation_count) {
		fprintf(
			stderr,
			"artifact graph density failed terms=%zu/%zu cases=%zu/%zu "
			"binders=%zu/%zu frames=%zu/%zu types=%zu/%zu parameters=%zu/%zu "
			"constructors=%zu/%zu fields=%zu/%zu exprs=%zu/%zu "
			"propositions=%zu/%zu claims=%zu/%zu derivations=%zu/%zu\n",
			present_term_count,
			terms->term_count,
			present_case_count,
			terms->case_count,
			present_case_binder_count,
			terms->case_binder_count,
			present_frame_count,
			terms->ih_scope_count,
			present_type_count,
			type_declarations->type_count,
			present_parameter_count,
			type_declarations->readback.parameter_count,
			present_constructor_count,
			type_declarations->constructor_count,
			present_field_type_count,
			type_declarations->readback.field_type_count,
			present_type_expr_count,
			type_declarations->readback.expr_count,
			present_proposition_count,
			judgement->proposition_count,
			present_claim_count,
			judgement->claim_count,
			present_derivation_count,
			judgement->derivation_count
		);
		return -1;
	}
	fprintf(
		stream,
		"counts terms %zu cases %zu case_binders %zu frames %zu types %zu parameters %zu constructors %zu field_types %zu type_exprs %zu propositions %zu claims %zu derivations %zu\n",
		terms->term_count,
		terms->case_count,
		terms->case_binder_count,
		terms->ih_scope_count,
		type_declarations->type_count,
		type_declarations->readback.parameter_count,
		type_declarations->constructor_count,
		type_declarations->readback.field_type_count,
		type_declarations->readback.expr_count,
		judgement->proposition_count,
		judgement->claim_count,
		judgement->derivation_count
	);

	fprintf(stream, "type_declarations %zu\n", present_type_count);
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		const char* type_name = artifact_optional_symbol_name(
			symbols, type->name_symbol_id
		);
		const char* namespace_name = artifact_optional_symbol_name(
			symbols, type->namespace_symbol_id
		);
		if (!type_name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"type_decl %zu %s %s %u %u %u %u %u %u %u %u %u %d %u\n",
			i,
			type_name,
			namespace_name,
			type->type_index,
			type->first_parameter,
			type->parameter_count,
			type->first_constructor,
			type->constructor_count,
			type->formation_classifier,
			artifact_context_slice_relocate(
				context_slice, type->parameter_context
			),
			artifact_context_slice_relocate(
				context_slice, type->index_context
			),
			type->index_count,
			type->origin_kind,
			type->origin_source_carrier_term_id
		);
	}

	fprintf(stream, "type_parameters %zu\n", present_parameter_count);
	for (size_t i = 0; i < type_declarations->readback.parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->readback.parameter_declarations[i];
		if (!artifact_parameter_present(parameter)) {
			continue;
		}
		fprintf(
			stream,
			"type_param %zu %u %s %u\n",
			i,
			parameter->binding_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}

	fprintf(stream, "type_constructors %zu\n", present_constructor_count);
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		const struct prototype_type_constructor_readback* readback =
			prototype_type_constructor_readback_get(type_declarations, (uint32_t)i);
		const struct prototype_constructor_classifier_cache_entry* cache =
			prototype_type_constructor_classifier_cache_get(
				type_declarations, (uint32_t)i
			);
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (!readback || !cache) {
			return -1;
		}
		const char* constructor_name = artifact_optional_symbol_name(
			symbols, constructor->name_symbol_id
		);
		if (!constructor_name) {
			return -1;
		}
		fprintf(
			stream,
			"type_constructor %zu %s %u %u %u %u %u %u %u %u %u\n",
			i,
			constructor_name,
				constructor->owner_type,
				constructor->constructor_index,
				readback->first_field_type,
				readback->field_count,
				readback->result_type,
				artifact_context_slice_relocate(
					context_slice, constructor->parameter_context
				),
				artifact_context_slice_relocate(
					context_slice, constructor->field_context
				),
				constructor->result_classifier,
				cache->classifier
			);
	}

	fprintf(stream, "type_field_refs %zu\n", present_field_type_count);
	for (size_t i = 0; i < type_declarations->readback.field_type_count; ++i) {
		if (!artifact_field_type_present(&type_declarations->readback.field_types[i])) {
			continue;
		}
		fprintf(stream, "type_field_ref %zu %u\n", i, type_declarations->readback.field_types[i]);
	}

	fprintf(stream, "type_exprs %zu\n", present_type_expr_count);
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (!artifact_type_expr_present(&type_declarations->readback.exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&type_declarations->readback.exprs[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "terms %zu\n", present_term_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (write_artifact_term(
				stream,
				symbols,
				intrinsic_environment,
				type_declarations,
				terms,
				(uint32_t)i,
				&terms->terms[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "match_cases %zu\n", present_case_count);
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		if (!artifact_case_present(match_case)) {
			continue;
		}
		fprintf(
			stream,
			"match_case %zu %s %u %u %u %u %u\n",
			i,
			symbol_to_string(symbols, terms->case_label_symbols[i]),
			match_case->constructor_owner,
			match_case->constructor_id,
			match_case->first_binder,
			match_case->binder_count,
			match_case->body
		);
	}

	fprintf(stream, "case_binders %zu\n", present_case_binder_count);
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (!artifact_case_binder_present(&terms->case_binders[i])) {
			continue;
		}
		fprintf(
			stream,
			"case_binder %zu %u %d\n",
			i,
			terms->case_binders[i].binding_id,
			terms->case_binders[i].is_recursive
		);
	}

	fprintf(stream, "match_frames %zu\n", present_frame_count);
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		const struct prototype_ih_scope* frame = &terms->ih_scopes[i];
		if (!artifact_frame_present(frame)) {
			continue;
		}
		if (frame->match_term >= terms->term_count ||
			terms->terms[frame->match_term].tag != PROTOTYPE_TERM_MATCH ||
			terms->terms[frame->match_term].as.match.scrutinee >=
				terms->term_count) {
			return -1;
		}
		const struct prototype_term* scrutinee = &terms->terms[
			terms->terms[frame->match_term].as.match.scrutinee
		];
		if (scrutinee->tag == PROTOTYPE_TERM_VAR &&
			(frame->scrutinee_binding_id == PROTOTYPE_INVALID_ID ||
			 scrutinee->as.var.binding_id != frame->scrutinee_binding_id)) {
			return -1;
		}
		fprintf(
			stream,
			"match_frame %zu %u %u %u %d ",
			i,
			frame->match_term,
			frame->scrutinee_binding_id,
			frame->key.case_count,
			frame->key.is_linkable
		);
		print_artifact_key(stream, &frame->key.match_key);
		fprintf(stream, "\n");
	}

	fprintf(stream, "propositions %zu\n", present_proposition_count);
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			&judgement->propositions[i];
		if (!artifact_candidate_claim_present(proposition)) {
			return -1;
		}
		fprintf(
			stream,
			"proposition %zu %d %d %u %u %u %u %u %u",
			i,
			proposition->kind,
			proposition->authority_kind,
			proposition->authority_id,
			artifact_context_slice_relocate(
				context_slice, proposition->context_id
			),
			proposition->occurrence_id,
			proposition->subject,
			proposition->classifier,
			proposition->resource_usage_count
		);
		for (uint32_t usage_index = 0;
			usage_index < proposition->resource_usage_count;
			++usage_index) {
			fprintf(
				stream,
				" %u %d",
				proposition->resource_usage[usage_index].binding_id,
				proposition->resource_usage[usage_index].grade
			);
		}
		fprintf(stream, "\n");
	}

	fprintf(stream, "claims %zu\n", present_claim_count);
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (!artifact_claim_present(judgement, claim)) {
			return -1;
		}
		fprintf(
			stream,
			"claim %zu proposition %u\n",
			i,
			claim->proposition_id
		);
	}
	fprintf(stream, "derivations %zu\n", present_derivation_count);
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		if ((derivation->proof_kind >=
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION &&
			 derivation->proof_kind <=
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS &&
			 derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX) ||
			derivation->proof_kind >
				PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION) {
			return -1;
		}
		fprintf(
			stream,
			"derivation %zu %d claim %u premises %u\n",
			i,
			derivation->proof_kind,
			derivation->conclusion_claim_id,
			derivation->premise_count
		);
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
			fprintf(
				stream,
				"payload constructor %u %u %u\n",
				derivation->rule_data.constructor.owner_view,
				derivation->rule_data.constructor.constructor_index,
				derivation->rule_data.constructor.field_index
			);
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
			fprintf(
				stream,
				"payload constructor_spine %u\n",
				derivation->rule_data.constructor.owner_view
			);
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			fprintf(
				stream,
				"payload induction %u %u %u %u\n",
				derivation->rule_data.induction.match,
				derivation->rule_data.induction.motive,
				derivation->rule_data.induction.case_index,
				derivation->rule_data.induction.field_index
			);
		} else {
			fprintf(stream, "payload none\n");
		}
		fprintf(
			stream,
			"action %d %u\n",
			derivation->semantic_action_kind,
			derivation->semantic_action_id
		);
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			const struct prototype_judgement_premise_edge* premise =
				&derivation->premises[j];
			if ((premise->claim_id == PROTOTYPE_INVALID_ID) ==
				(premise->scoped_proposition_id == PROTOTYPE_INVALID_ID)) {
				return -1;
			}
			fprintf(
				stream,
				"premise %s %u action %d %u\n",
				premise->claim_id != PROTOTYPE_INVALID_ID ? "claim" : "scoped",
				premise->claim_id != PROTOTYPE_INVALID_ID ?
					premise->claim_id : premise->scoped_proposition_id,
				premise->semantic_action_kind,
				premise->semantic_action_id
			);
		}
	}
	fprintf(stream, "END graph\n");
	return 0;
}

static int artifact_term_reaches_term_at_depth(
	const struct prototype_term_db* terms,
	uint32_t root,
	uint32_t needle,
	uint32_t depth
) {
	if (!terms || root >= terms->term_count || needle >= terms->term_count ||
		depth > 256) {
		return -1;
	}
	if (root == needle) {
		return 1;
	}
	const struct prototype_term* term = &terms->terms[root];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.constructor.owner,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_APP: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.function,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.argument,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.lambda.body,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_PI: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.domain,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.codomain_family,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.match.scrutinee,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count) {
					return -1;
				}
				found = artifact_term_reaches_term_at_depth(
					terms,
					terms->cases[case_id].body,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW:
			{
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.core,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.source,
					needle,
					depth + 1
				);
			}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.induction_hypothesis.argument,
					needle,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.computation_type.label,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.computation_type.result,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.left, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.right, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_forall.body, needle, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_operation.latent_row, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk_type.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.return_term.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.force.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.computation_fold.computation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.computation_fold.return_clause, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				found = artifact_term_reaches_term_at_depth(
					terms, clause->operation, needle, depth + 1
				);
				if (found != 0) {
					return found;
				}
				found = artifact_term_reaches_term_at_depth(
					terms, clause->body, needle, depth + 1
				);
				if (found != 0) {
					return found;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.operation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.argument, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.continuation, needle, depth + 1
			);
		}
			default:
				return 0;
	}
}

static int artifact_external_term_ref_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t term_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		term_id >= terms->term_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		int found = 0;
		if (export->local_term < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->local_term, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
		if (export->classifier < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->classifier, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		int found = artifact_term_reaches_term_at_depth(terms, relation->subject, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
		found = artifact_term_reaches_term_at_depth(terms, relation->classifier, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (!artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (type_declarations->readback.exprs[i].tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(
				type_declarations,
				type_declarations->readback.exprs[i].as.name.symbol_id
			)) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (type_declarations->readback.exprs[i].tag == PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_match_case_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t case_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		case_id >= terms->case_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_MATCH ||
			case_id < term->as.match.first_case ||
			case_id >= term->as.match.first_case + term->as.match.case_count) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_resolved_constructor_owner_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			terms->terms[i].as.constructor.owner != PROTOTYPE_INVALID_ID) {
			if (artifact_external_term_ref_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		int reachable = 0;
		if (terms->cases[i].constructor_owner != PROTOTYPE_INVALID_ID) {
			if (artifact_match_case_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	return 0;
}

static int write_artifact_relocation_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t external_term_ref_count;
	size_t resolved_external_term_ref_count;
	size_t external_type_expr_ref_count;
	size_t resolved_external_type_expr_ref_count;
	size_t resolved_constructor_owner_ref_count;
	if (artifact_count_external_term_refs(
			interface,
			terms,
			judgement,
			&external_term_ref_count
		) != 0 ||
		artifact_count_resolved_external_term_refs(
			interface,
			terms,
			judgement,
			&resolved_external_term_ref_count
		) != 0 ||
		artifact_count_external_type_expr_refs(type_declarations, &external_type_expr_ref_count) != 0 ||
		artifact_count_resolved_external_type_expr_refs(
			type_declarations,
			&resolved_external_type_expr_ref_count
		) != 0 ||
		artifact_count_resolved_constructor_owner_refs(
			interface,
			terms,
			judgement,
			&resolved_constructor_owner_ref_count
		) != 0) {
		return -1;
	}

	fprintf(stream, "SECTION relocation\n");
	fprintf(stream, "external_term_refs %zu\n", external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, term->as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"external_term_ref %zu %s %s\n",
			i,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "resolved_external_term_refs %zu\n", resolved_external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		uint32_t export_id;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (prototype_artifact_interface_find_term_export_in_namespace(
					interface,
					term->as.external_ref.name.namespace_symbol_id,
					term->as.external_ref.name.name_symbol_id,
				&export_id
			) != 0) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_external_term_ref %zu %u %s %s\n",
			i,
			export_id,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "external_type_expr_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->readback.exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_expr_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}

	fprintf(stream, "resolved_external_type_expr_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->readback.exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_expr_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_representation_fingerprint(stream, &expr->as.imported_type.representation_fingerprint);
		fprintf(stream, "\n");
	}

	fprintf(stream, "external_type_former_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->readback.exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_former_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}
	fprintf(stream, "resolved_external_type_former_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->readback.exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_former_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_representation_fingerprint(stream, &expr->as.imported_type.representation_fingerprint);
		fprintf(stream, "\n");
	}
	fprintf(stream, "resolved_constructor_owner_refs %zu\n", resolved_constructor_owner_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR ||
			term->as.constructor.owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				term->as.constructor.owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 1 %zu %u %u ",
			i,
			term->as.constructor.owner,
			term->as.constructor.constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_match_case_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				match_case->constructor_owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 2 %zu %u %u ",
			i,
			match_case->constructor_owner,
			match_case->constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	fprintf(stream, "external_constructor_owner_refs 0\n");
	fprintf(stream, "END relocation\n");
	return 0;
}

static int write_artifact_universe_section(
	FILE* stream,
	const struct prototype_universe_db* universe
) {
	if (!stream || !universe) {
		return -1;
	}
	fprintf(stream, "SECTION universe\n");
	size_t present_node_count = 0;
	size_t present_edge_count = 0;
	for (size_t i = 0; i < universe->node_count; ++i) {
		if (artifact_universe_node_present(&universe->nodes[i])) {
			present_node_count++;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		if (artifact_universe_edge_present(&universe->edges[i])) {
			present_edge_count++;
		}
	}
	fprintf(
		stream,
		"counts node_slots %zu nodes %zu edge_slots %zu edges %zu levels %zu constraints %zu solved %d\n",
		universe->node_count,
		present_node_count,
		universe->edge_count,
		present_edge_count,
		universe->level_count,
		universe->constraint_count,
		universe->solved
	);
	fprintf(stream, "universe_nodes %zu\n", present_node_count);
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (!artifact_universe_node_present(node)) {
			continue;
		}
		fprintf(
			stream,
			"universe_node %zu %d %u %u %d %u\n",
			i,
			node->tag,
			node->type_id,
			node->parameter_id,
			node->symbol_id,
			node->type_expr
		);
	}
	fprintf(stream, "universe_edges %zu\n", present_edge_count);
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (!artifact_universe_edge_present(edge)) {
			continue;
		}
		fprintf(
			stream,
			"universe_edge %zu %d %u %u\n",
			i,
			edge->tag,
			edge->from_node,
			edge->to_node
		);
	}
	fprintf(stream, "universe_levels %zu\n", universe->level_count);
	for (size_t i = 0; i < universe->level_count; ++i) {
		const struct prototype_universe_level* level = &universe->levels[i];
		fprintf(
			stream,
			"universe_level %zu %u %d\n",
			i,
			level->level_var,
			level->value
		);
	}
	fprintf(stream, "universe_constraints %zu\n", universe->constraint_count);
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		fprintf(
			stream,
			"universe_constraint %zu %u %u %d %u %u %d %u %d %u %u %u\n",
			i,
			constraint->lower_level_var,
			constraint->upper_level_var,
			constraint->offset,
			constraint->subject,
			constraint->classifier,
			constraint->reason,
			constraint->source_claim_id,
			constraint->source_authority_kind,
			constraint->source_authority_id,
			constraint->source_subject,
			constraint->source_classifier
		);
	}
	fprintf(stream, "END universe\n");
	return 0;
}

static int write_artifact_debug_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_ast_db* asts
) {
	if (!stream || !symbols || !interface) {
		return -1;
	}
	fprintf(stream, "SECTION debug\n");
	fprintf(stream, "term_names %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		uint32_t source_entry_id = PROTOTYPE_INVALID_ID;
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->assignment_count; ++j) {
				const struct prototype_ast_term_assignment_def* assignment =
					&asts->assignments[j];
				if (assignment->name_symbol_id != export->name_symbol_id ||
					assignment->compiled_term != export->local_term) {
					continue;
				}
				source_entry_id = assignment->source_entry_id;
				name_span = assignment->name_span;
				body_span = assignment->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"term_name %s %u %u %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_term,
			export->classifier,
			source_entry_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "type_names %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != export->local_type_id) {
					continue;
				}
				name_span = type->name_span;
				body_span = type->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"type_name %s %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_type_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "constructor_names %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		struct prototype_source_span name_span;
		memset(&name_span, 0, sizeof(name_span));
		if (asts && export->type_export_index < interface->type_export_count) {
			const struct prototype_artifact_type_export* type_export =
				&interface->type_exports[export->type_export_index];
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != type_export->local_type_id ||
					export->ordinal >= type->constructor_count) {
					continue;
				}
				uint32_t constructor_id = type->first_constructor + export->ordinal;
				if (constructor_id >= asts->type_constructor_count) {
					continue;
				}
				name_span = asts->type_constructors[constructor_id].name_span;
				break;
			}
		}
		fprintf(
			stream,
			"constructor_name %u %s %u %u %u\n",
			export->type_export_index,
			symbol_to_string(symbols, export->name_symbol_id),
			export->ordinal,
			name_span.line,
			name_span.column
		);
	}
	fprintf(stream, "END debug\n");
	return 0;
}

/* This graph retains source IDs only while computing reachability. It is not
 * a serializable artifact graph. Publication must relocate it into dense,
 * independently numbered arenas before writing. */
