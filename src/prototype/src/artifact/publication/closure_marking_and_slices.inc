struct artifact_graph_marks {
	const struct prototype_type_declaration_db* type_declarations;
	const struct prototype_judgement_db* judgement;
	const struct prototype_cwf_certificate_db* cwf_certificates;
	const uint32_t* source_substitution_evidence_claims;
	size_t source_substitution_evidence_count;
	uint32_t* substitution_evidence_claims;
	unsigned char* terms;
	unsigned char* cases;
	unsigned char* case_binders;
	unsigned char* frames;
	unsigned char* types;
	unsigned char* parameters;
	unsigned char* constructors;
	unsigned char* readback_field_types;
	unsigned char* type_exprs;
	unsigned char* propositions;
	unsigned char* claims;
	unsigned char* derivations;
	unsigned char* substitutions;
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
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t frame_count;
	size_t type_count;
	size_t parameter_count;
	size_t constructor_count;
	size_t readback_field_type_count;
	size_t type_expr_count;
	size_t proposition_count;
	size_t claim_count;
	size_t derivation_count;
	size_t substitution_count;
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

struct artifact_context_slice {
	unsigned char* reachable;
	uint32_t* relocation;
	size_t source_count;
	size_t context_count;
};

struct artifact_substitution_slice {
	unsigned char* reachable;
	uint32_t* relocation;
	size_t source_count;
	size_t substitution_count;
};

static int artifact_derivation_present(
	const struct prototype_judgement_derivation* derivation
);

static void artifact_context_slice_free(struct artifact_context_slice* slice) {
	if (!slice) {
		return;
	}
	free(slice->reachable);
	free(slice->relocation);
	memset(slice, 0, sizeof(*slice));
}

static void artifact_substitution_slice_free(
	struct artifact_substitution_slice* slice
) {
	if (!slice) {
		return;
	}
	free(slice->reachable);
	free(slice->relocation);
	memset(slice, 0, sizeof(*slice));
}

static int artifact_substitution_slice_mark(
	struct artifact_substitution_slice* slice,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t depth
) {
	if (!slice || !substitutions || depth > 512 ||
		substitution_id >= slice->source_count) {
		return -1;
	}
	if (slice->reachable[substitution_id] == 1) {
		return 0;
	}
	if (slice->reachable[substitution_id] == 2) {
		return -1;
	}
	slice->reachable[substitution_id] = 2;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		if (artifact_substitution_slice_mark(
				slice, substitutions, substitution->first, depth + 1
			) != 0) {
			return -1;
		}
	} else if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		if (artifact_substitution_slice_mark(
				slice, substitutions, substitution->first, depth + 1
			) != 0 || artifact_substitution_slice_mark(
				slice, substitutions, substitution->second, depth + 1
			) != 0) {
			return -1;
		}
	}
	slice->reachable[substitution_id] = 1;
	return 0;
}

static int artifact_substitution_slice_init(
	struct artifact_substitution_slice* slice,
	const struct prototype_judgement_db* judgement,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_compile_metadata* metadata
) {
	if (!slice || !judgement || !substitutions || !metadata) {
		return -1;
	}
	memset(slice, 0, sizeof(*slice));
	slice->source_count = substitutions->substitution_count;
	slice->reachable = calloc(
		slice->source_count == 0 ? 1 : slice->source_count,
		sizeof(*slice->reachable)
	);
	slice->relocation = malloc(
		(slice->source_count == 0 ? 1 : slice->source_count) *
			sizeof(*slice->relocation)
	);
	if (!slice->reachable || !slice->relocation) {
		artifact_substitution_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, (uint32_t)i);
		if (!derivation) {
			continue;
		}
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			artifact_substitution_slice_mark(
				slice, substitutions, derivation->semantic_action_id, 0
			) != 0) {
			artifact_substitution_slice_free(slice);
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				artifact_substitution_slice_mark(
					slice,
					substitutions,
					derivation->premises[j].semantic_action_id,
					0
				) != 0) {
				artifact_substitution_slice_free(slice);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&metadata->typed_occurrences.occurrences[i];
		if (operation->context_action_substitution != PROTOTYPE_INVALID_ID &&
			artifact_substitution_slice_mark(
				slice,
				substitutions,
				operation->context_action_substitution,
				0
			) != 0) {
			artifact_substitution_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < metadata->typed_occurrences.case_count; ++i) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&metadata->typed_occurrences.cases[i];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
			artifact_substitution_slice_mark(
				slice,
				substitutions,
				operation_case->refinement_substitution,
				0
			) != 0) {
			artifact_substitution_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		if (slice->reachable[i]) {
			slice->relocation[i] = (uint32_t)slice->substitution_count++;
		}
	}
	return 0;
}

static int artifact_substitution_slice_canonicalize(
	struct artifact_substitution_slice* slice,
	const uint32_t* order,
	size_t order_count
) {
	if (!slice || (order_count > 0 && !order)) {
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	slice->substitution_count = 0;
	for (size_t position = 0; position < order_count; ++position) {
		uint32_t substitution_id = order[position];
		if (substitution_id >= slice->source_count ||
			!slice->reachable[substitution_id] ||
			slice->relocation[substitution_id] != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		slice->relocation[substitution_id] =
			(uint32_t)slice->substitution_count++;
	}
	for (uint32_t substitution_id = 0;
		substitution_id < slice->source_count;
		++substitution_id) {
		if (slice->reachable[substitution_id] &&
			slice->relocation[substitution_id] == PROTOTYPE_INVALID_ID) {
			slice->relocation[substitution_id] =
				(uint32_t)slice->substitution_count++;
		}
	}
	return 0;
}

static int artifact_context_slice_mark(
	struct artifact_context_slice* slice,
	const struct prototype_context_db* contexts,
	uint32_t context_id
) {
	if (!slice || !contexts || context_id >= contexts->context_count) {
		return -1;
	}
	while (context_id != prototype_context_empty(contexts)) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context) {
			return -1;
		}
		slice->reachable[context_id] = 1;
		context_id = context->parent;
	}
	slice->reachable[prototype_context_empty(contexts)] = 1;
	return 0;
}

static int artifact_context_slice_init(
	struct artifact_context_slice* slice,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const struct artifact_substitution_slice* substitution_slice
) {
	if (!slice || !type_declarations || !judgement || !metadata ||
		metadata->contexts.context_count == 0) {
		return -1;
	}
	memset(slice, 0, sizeof(*slice));
	slice->source_count = metadata->contexts.context_count;
	slice->reachable = calloc(slice->source_count, sizeof(*slice->reachable));
	slice->relocation = malloc(
		slice->source_count * sizeof(*slice->relocation)
	);
	if (!slice->reachable || !slice->relocation) {
		artifact_context_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	if (artifact_context_slice_mark(
		slice, &metadata->contexts, prototype_context_empty(&metadata->contexts)
	) != 0) {
		artifact_context_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < type_declarations->semantic_schema.type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		if (artifact_context_slice_mark(
				slice, &metadata->contexts, type->parameter_context
			) != 0 || artifact_context_slice_mark(
				slice, &metadata->contexts, type->index_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->semantic_schema.constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[i];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (artifact_context_slice_mark(
				slice, &metadata->contexts, constructor->parameter_context
			) != 0 || artifact_context_slice_mark(
				slice, &metadata->contexts, constructor->field_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, (uint32_t)i);
		if (proposition &&
			artifact_context_slice_mark(
				slice,
				&metadata->contexts,
				proposition->context_id
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[j]
				);
			if (!proposition || artifact_context_slice_mark(
					slice, &metadata->contexts, proposition->context_id
				) != 0) {
				artifact_context_slice_free(slice);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		if (artifact_context_slice_mark(
			slice, &metadata->contexts, metadata->typed_occurrences.occurrences[i].context_id
		) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0;
		i < metadata->substitutions.substitution_count;
		++i) {
		if (substitution_slice &&
			(i >= substitution_slice->source_count ||
			 !substitution_slice->reachable[i])) {
			continue;
		}
		const struct prototype_substitution* substitution =
			&metadata->substitutions.substitutions[i];
		if (artifact_context_slice_mark(
				slice, &metadata->contexts, substitution->source_context
			) != 0 || artifact_context_slice_mark(
				slice, &metadata->contexts, substitution->target_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (uint32_t i = 0; i < slice->source_count; ++i) {
		if (slice->reachable[i]) {
			slice->relocation[i] = (uint32_t)slice->context_count++;
		}
	}
	return 0;
}

static int artifact_context_slice_assign_path(
	struct artifact_context_slice* slice,
	const struct prototype_context_db* contexts,
	uint32_t context_id
) {
	if (!slice || !contexts || context_id >= slice->source_count ||
		!slice->reachable[context_id]) {
		return -1;
	}
	if (slice->relocation[context_id] != PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t empty = prototype_context_empty(contexts);
	if (context_id != empty) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context || artifact_context_slice_assign_path(
				slice, contexts, context->parent
			) != 0) {
			return -1;
		}
	}
	slice->relocation[context_id] = (uint32_t)slice->context_count++;
	return 0;
}

static int artifact_context_slice_canonicalize(
	struct artifact_context_slice* slice,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const uint32_t* type_order,
	size_t type_order_count,
	const uint32_t* constructor_order,
	size_t constructor_order_count,
	const uint32_t* claim_order,
	size_t claim_order_count,
	const uint32_t* derivation_order,
	size_t derivation_order_count,
	const uint32_t* substitution_order,
	size_t substitution_order_count
) {
	if (!slice || !type_declarations || !judgement || !metadata) {
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	slice->context_count = 0;
	if (artifact_context_slice_assign_path(
			slice,
			&metadata->contexts,
			prototype_context_empty(&metadata->contexts)
		) != 0) {
		return -1;
	}
	for (size_t position = 0; position < type_order_count; ++position) {
		uint32_t type_id = type_order[position];
		if (type_id >= type_declarations->semantic_schema.type_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[type_id];
		if (type->type_index == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_context_slice_assign_path(
				slice, &metadata->contexts, type->parameter_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, type->index_context
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < constructor_order_count; ++position) {
		uint32_t constructor_id = constructor_order[position];
		if (constructor_id >= type_declarations->semantic_schema.constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[constructor_id];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (artifact_context_slice_assign_path(
				slice, &metadata->contexts, constructor->parameter_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, constructor->field_context
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < claim_order_count; ++position) {
		uint32_t claim_id = claim_order[position];
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (!proposition || artifact_context_slice_assign_path(
				slice, &metadata->contexts, proposition->context_id
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < derivation_order_count; ++position) {
		uint32_t derivation_id = derivation_order[position];
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, derivation_id);
		if (!derivation) {
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[j]
				);
			if (!proposition || artifact_context_slice_assign_path(
					slice, &metadata->contexts, proposition->context_id
				) != 0) {
				return -1;
			}
		}
	}
	for (size_t position = 0; position < substitution_order_count; ++position) {
		uint32_t substitution_id = substitution_order[position];
		const struct prototype_substitution* substitution =
			prototype_substitution_get(&metadata->substitutions, substitution_id);
		if (!substitution || artifact_context_slice_assign_path(
				slice, &metadata->contexts, substitution->source_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, substitution->target_context
			) != 0) {
			return -1;
		}
	}
	for (uint32_t context_id = 0; context_id < slice->source_count; ++context_id) {
		if (slice->reachable[context_id] &&
			artifact_context_slice_assign_path(
				slice, &metadata->contexts, context_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static uint32_t artifact_context_slice_relocate(
	const struct artifact_context_slice* slice,
	uint32_t context_id
) {
	if (!slice || context_id >= slice->source_count ||
		!slice->reachable[context_id]) {
		return PROTOTYPE_INVALID_ID;
	}
	return slice->relocation[context_id];
}

static int artifact_record_publication_order(
	uint32_t* order,
	size_t capacity,
	size_t* count,
	uint32_t id
) {
	if (!order || !count || *count >= capacity || id >= capacity) {
		return -1;
	}
	order[(*count)++] = id;
	return 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
);
static int artifact_mark_accepted_claim(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t depth
);

static int artifact_derivation_semantic_compare(
	const struct prototype_judgement_derivation* left,
	const struct prototype_judgement_derivation* right
) {
#define COMPARE_DERIVATION_FIELD(a, b) \
	do { \
		if ((a) != (b)) { \
			return (a) < (b) ? -1 : 1; \
		} \
	} while (0)
	COMPARE_DERIVATION_FIELD(left->proof_kind, right->proof_kind);
	COMPARE_DERIVATION_FIELD(
		left->conclusion_claim_id, right->conclusion_claim_id
	);
	for (uint32_t i = 0; i < 4; ++i) {
		COMPARE_DERIVATION_FIELD(
			left->rule_data.words[i], right->rule_data.words[i]
		);
	}
	COMPARE_DERIVATION_FIELD(
		left->semantic_action_kind, right->semantic_action_kind
	);
	COMPARE_DERIVATION_FIELD(
		left->semantic_action_id, right->semantic_action_id
	);
	COMPARE_DERIVATION_FIELD(left->premise_count, right->premise_count);
	for (uint32_t i = 0; i < left->premise_count; ++i) {
		COMPARE_DERIVATION_FIELD(
			left->premises[i].claim_id, right->premises[i].claim_id
		);
		COMPARE_DERIVATION_FIELD(
			left->premises[i].scoped_proposition_id,
			right->premises[i].scoped_proposition_id
		);
		COMPARE_DERIVATION_FIELD(
			left->premises[i].semantic_action_kind,
			right->premises[i].semantic_action_kind
		);
		COMPARE_DERIVATION_FIELD(
			left->premises[i].semantic_action_id,
			right->premises[i].semantic_action_id
		);
	}
#undef COMPARE_DERIVATION_FIELD
	return 0;
}

static int artifact_claim_has_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (!judgement || claim_id >= judgement->claim_count) {
		return 0;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, i);
		if (derivation && derivation->conclusion_claim_id == claim_id) {
			return 1;
		}
	}
	return 0;
}

static int artifact_mark_substitution(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t depth
) {
	if (!marks || !terms || !substitutions || depth > 512 ||
		substitution_id >= substitutions->substitution_count ||
		substitution_id >= marks->substitution_count) {
		fprintf(stderr,
			"artifact closure: invalid substitution traversal substitution=%u depth=%u "
			"source_count=%zu mark_count=%zu\n",
			substitution_id,
			depth,
			substitutions ? substitutions->substitution_count : 0,
			marks ? marks->substitution_count : 0);
		return -1;
	}
	if (marks->substitutions[substitution_id] == 1) {
		return 0;
	}
	if (marks->substitutions[substitution_id] == 2) {
		fprintf(stderr, "artifact closure: cyclic substitution dependency substitution=%u\n",
			substitution_id);
		return -1;
	}
	marks->substitutions[substitution_id] = 2;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution ||
		(substitution->term != PROTOTYPE_INVALID_ID && artifact_mark_term(
			marks, terms, substitution->term, depth + 1
		) != 0) ||
		(substitution->term_classifier != PROTOTYPE_INVALID_ID &&
		 artifact_mark_term(
			marks, terms, substitution->term_classifier, depth + 1
		 ) != 0)) {
		fprintf(stderr,
			"artifact closure: substitution term traversal failed substitution=%u "
			"kind=%d term=%u classifier=%u\n",
			substitution_id,
			substitution ? substitution->kind : 0,
			substitution ? substitution->term : PROTOTYPE_INVALID_ID,
			substitution ? substitution->term_classifier : PROTOTYPE_INVALID_ID);
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		if (artifact_mark_substitution(
				marks, terms, substitutions, substitution->first, depth + 1
			) != 0) {
			fprintf(stderr,
				"artifact closure: substitution prefix traversal failed substitution=%u prefix=%u\n",
				substitution_id, substitution->first);
			return -1;
		}
	} else if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		if (artifact_mark_substitution(
				marks, terms, substitutions, substitution->first, depth + 1
			) != 0 || artifact_mark_substitution(
				marks, terms, substitutions, substitution->second, depth + 1
			) != 0) {
			fprintf(stderr,
				"artifact closure: composed substitution traversal failed substitution=%u "
				"first=%u second=%u\n",
				substitution_id, substitution->first, substitution->second);
			return -1;
		}
	}
	marks->substitutions[substitution_id] = 1;
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		uint32_t evidence_claim = PROTOTYPE_INVALID_ID;
		if (substitution_id < marks->source_substitution_evidence_count &&
			marks->source_substitution_evidence_claims &&
			marks->source_substitution_evidence_claims[substitution_id] !=
				PROTOTYPE_INVALID_ID) {
			evidence_claim = marks->source_substitution_evidence_claims[
				substitution_id
			];
			if (!prototype_cwf_substitution_claim_certifies(
					substitutions,
					marks->judgement,
					substitution_id,
					evidence_claim
				) || !artifact_claim_has_derivation(
					marks->judgement, evidence_claim
				)) {
				return -1;
			}
		} else if (marks->cwf_certificates) {
			if (prototype_cwf_certificate_db_substitution_evidence(
					marks->cwf_certificates,
					substitutions,
					marks->judgement,
					substitution_id,
					&evidence_claim
				) != 0 || !artifact_claim_has_derivation(
					marks->judgement, evidence_claim
				)) {
				evidence_claim = PROTOTYPE_INVALID_ID;
			}
		}
		if (evidence_claim == PROTOTYPE_INVALID_ID) {
			for (uint32_t i = 0; i < marks->judgement->claim_count; ++i) {
				if (!prototype_cwf_substitution_claim_certifies(
						substitutions, marks->judgement, substitution_id, i
					) || !artifact_claim_has_derivation(marks->judgement, i)) {
					continue;
				}
				if (evidence_claim == PROTOTYPE_INVALID_ID) {
					evidence_claim = i;
				}
			}
		}
		if (evidence_claim == PROTOTYPE_INVALID_ID ||
			artifact_mark_accepted_claim(
				marks,
				terms,
				marks->judgement,
				evidence_claim,
				depth + 1
			) != 0) {
			fprintf(stderr,
				"artifact closure: substitution evidence unavailable substitution=%u claim=%u "
				"source=%u target=%u prefix=%u term=%u tag=%d classifier=%u\n",
				substitution_id,
				evidence_claim,
				substitution->source_context,
				substitution->target_context,
				substitution->first,
				substitution->term,
				substitution->term < terms->term_count ?
					terms->terms[substitution->term].tag : -1,
				substitution->term_classifier);
			return -1;
		}
		marks->substitution_evidence_claims[substitution_id] = evidence_claim;
	}
	return artifact_record_publication_order(
		marks->substitution_order,
		marks->substitution_count,
		&marks->ordered_substitution_count,
		substitution_id
	);
}

static uint32_t artifact_term_publication_rank(
	const struct artifact_graph_marks* marks,
	uint32_t term_id
) {
	if (term_id == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < marks->ordered_term_count; ++i) {
		if (marks->term_order[i] == term_id) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int artifact_substitution_candidate_is_ready(
	const struct prototype_substitution* substitution,
	const uint32_t* ranks,
	size_t rank_count
) {
	if (!substitution || !ranks) {
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		return substitution->first < rank_count &&
			ranks[substitution->first] != PROTOTYPE_INVALID_ID;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return substitution->first < rank_count &&
			substitution->second < rank_count &&
			ranks[substitution->first] != PROTOTYPE_INVALID_ID &&
			ranks[substitution->second] != PROTOTYPE_INVALID_ID;
	}
	return 1;
}

static int artifact_substitution_candidate_compare(
	const struct artifact_graph_marks* marks,
	const struct prototype_substitution* left,
	uint32_t left_id,
	const struct prototype_substitution* right,
	uint32_t right_id,
	const uint32_t* ranks
) {
#define COMPARE_FIELD(a, b) \
	do { \
		if ((a) != (b)) { \
			return (a) < (b) ? -1 : 1; \
		} \
	} while (0)
	COMPARE_FIELD(left->kind, right->kind);
	COMPARE_FIELD(left->source_context, right->source_context);
	COMPARE_FIELD(left->target_context, right->target_context);
	uint32_t left_first = left->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
		left->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[left->first] : PROTOTYPE_INVALID_ID;
	uint32_t right_first = right->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
		right->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[right->first] : PROTOTYPE_INVALID_ID;
	COMPARE_FIELD(left_first, right_first);
	uint32_t left_second = left->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[left->second] : PROTOTYPE_INVALID_ID;
	uint32_t right_second = right->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[right->second] : PROTOTYPE_INVALID_ID;
	COMPARE_FIELD(left_second, right_second);
	COMPARE_FIELD(
		artifact_term_publication_rank(marks, left->term),
		artifact_term_publication_rank(marks, right->term)
	);
	COMPARE_FIELD(
		artifact_term_publication_rank(marks, left->term_classifier),
		artifact_term_publication_rank(marks, right->term_classifier)
	);
	COMPARE_FIELD(left_id, right_id);
#undef COMPARE_FIELD
	return 0;
}

static int artifact_canonicalize_substitution_order(
	struct artifact_graph_marks* marks,
	const struct prototype_substitution_db* substitutions
) {
	if (!marks || !substitutions ||
		marks->substitution_count != substitutions->substitution_count) {
		return -1;
	}
	uint32_t* ranks = malloc(
		(marks->substitution_count == 0 ? 1 : marks->substitution_count) *
			sizeof(*ranks)
	);
	uint32_t* order = malloc(
		(marks->ordered_substitution_count == 0 ? 1 :
		 marks->ordered_substitution_count) * sizeof(*order)
	);
	if (!ranks || !order) {
		free(ranks);
		free(order);
		return -1;
	}
	for (size_t i = 0; i < marks->substitution_count; ++i) {
		ranks[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t position = 0;
		position < marks->ordered_substitution_count;
		++position) {
		uint32_t best = PROTOTYPE_INVALID_ID;
		for (uint32_t candidate = 0;
			candidate < marks->substitution_count;
			++candidate) {
			if (!marks->substitutions[candidate] ||
				ranks[candidate] != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_substitution* substitution =
				prototype_substitution_get(substitutions, candidate);
			if (!artifact_substitution_candidate_is_ready(
					substitution, ranks, marks->substitution_count
				)) {
				continue;
			}
			if (best == PROTOTYPE_INVALID_ID ||
				artifact_substitution_candidate_compare(
					marks,
					substitution,
					candidate,
					prototype_substitution_get(substitutions, best),
					best,
					ranks
				) < 0) {
				best = candidate;
			}
		}
		if (best == PROTOTYPE_INVALID_ID) {
			free(ranks);
			free(order);
			return -1;
		}
		order[position] = best;
		ranks[best] = (uint32_t)position;
	}
	memcpy(
		marks->substitution_order,
		order,
		marks->ordered_substitution_count * sizeof(*order)
	);
	free(ranks);
	free(order);
	return 0;
}

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
);

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
);

static int artifact_mark_type_expr(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t expr_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (expr_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (expr_id >= marks->type_expr_count || expr_id >= type_declarations->readback.expr_count) {
		return -1;
	}
	const struct prototype_type_expr* expr = &type_declarations->readback.exprs[expr_id];
	if (!artifact_type_expr_present(expr)) {
		return -1;
	}
	if (marks->type_exprs[expr_id]) {
		return 0;
	}
	marks->type_exprs[expr_id] = 1;
	if (artifact_record_publication_order(
			marks->type_expr_order,
			marks->type_expr_count,
			&marks->ordered_type_expr_count,
			expr_id
		) != 0) {
		return -1;
	}
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_NAME: {
			const struct prototype_type_declaration* local_type =
				prototype_type_declaration_lookup(
					type_declarations,
					expr->as.name.symbol_id
				);
			if (local_type) {
				return artifact_mark_type(
					marks,
					terms,
					local_type->type_index,
					depth + 1
				);
			}
			return 0;
		}
		case PROTOTYPE_TYPE_EXPR_LOCAL_TYPE_MEMBER: {

			const struct prototype_type_declaration* local_type =
				prototype_type_declaration_lookup(
					type_declarations,
					expr->as.local_type_member.owner_symbol_id
				);
			if (!local_type || !prototype_type_declaration_lookup_constructor(
					type_declarations,
					local_type->type_index,
					expr->as.local_type_member.member_symbol_id
				)) {
				return -1;
			}
			return artifact_mark_type(
				marks, terms, local_type->type_index, depth + 1
			);
		}
		case PROTOTYPE_TYPE_EXPR_APP:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.app.function,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.app.argument,
					depth + 1
				) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.arrow.domain,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.arrow.codomain,
					depth + 1
				) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_PI:
			return artifact_mark_type_expr(
				marks, terms, expr->as.pi.domain, depth + 1
			) == 0 && artifact_mark_type_expr(
				marks, terms, expr->as.pi.codomain, depth + 1
			) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_COMPUTATION_REFERENCE:
			return artifact_mark_type_expr(
				marks, terms, expr->as.computation_reference.result, depth + 1
			);
		default:
			return 0;
	}
}

static int artifact_mark_parameter(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t parameter_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		parameter_id >= marks->parameter_count ||
		parameter_id >= type_declarations->readback.parameter_count) {
		return -1;
	}
	const struct prototype_type_parameter_declaration* parameter =
		&type_declarations->readback.parameter_declarations[parameter_id];
	if (!artifact_parameter_present(parameter)) {
		return -1;
	}
	if (marks->parameters[parameter_id]) {
		return 0;
	}
	marks->parameters[parameter_id] = 1;
	if (artifact_record_publication_order(
			marks->parameter_order,
			marks->parameter_count,
			&marks->ordered_parameter_count,
			parameter_id
		) != 0) {
		return -1;
	}
	return artifact_mark_type_expr(marks, terms, parameter->type_expr, depth + 1);
}

static int artifact_mark_field_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t field_type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		field_type_id >= marks->readback_field_type_count ||
		field_type_id >= type_declarations->readback.field_type_count) {
		return -1;
	}
	const uint32_t* field_type = &type_declarations->readback.field_types[field_type_id];
	if (!artifact_field_type_present(field_type)) {
		return -1;
	}
	if (marks->readback_field_types[field_type_id]) {
		return 0;
	}
	marks->readback_field_types[field_type_id] = 1;
	if (artifact_record_publication_order(
			marks->readback_field_type_order,
			marks->readback_field_type_count,
			&marks->ordered_readback_field_type_count,
			field_type_id
		) != 0) {
		return -1;
	}
	return artifact_mark_type_expr(marks, terms, *field_type, depth + 1);
}

static int artifact_mark_constructor(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t constructor_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		constructor_id >= marks->constructor_count ||
		constructor_id >= type_declarations->semantic_schema.constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->semantic_schema.constructor_declarations[constructor_id];
	const struct prototype_type_constructor_readback* readback =
		prototype_type_constructor_readback_get(type_declarations, constructor_id);
	const struct prototype_constructor_classifier_cache_entry* cache =
		prototype_type_constructor_classifier_cache_get(
			type_declarations, constructor_id
		);
	if (!artifact_constructor_present(constructor) || !readback || !cache) {
		return -1;
	}
	if (marks->constructors[constructor_id]) {
		return 0;
	}
	marks->constructors[constructor_id] = 1;
	if (artifact_record_publication_order(
			marks->constructor_order,
			marks->constructor_count,
			&marks->ordered_constructor_count,
			constructor_id
		) != 0) {
		return -1;
	}
	if (artifact_mark_type(marks, terms, constructor->owner_type, depth + 1) != 0) {
		return -1;
	}
	if (readback->result_type != PROTOTYPE_INVALID_ID &&
		artifact_mark_type_expr(
			marks, terms, readback->result_type, depth + 1
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < readback->field_count; ++i) {
		uint32_t field_id = readback->first_field_type + i;
		if (readback->first_field_type == PROTOTYPE_INVALID_ID ||
			field_id >= type_declarations->readback.field_type_count ||
			!artifact_field_type_present(
				&type_declarations->readback.field_types[field_id]
			)) {
			continue;
		}
		if (artifact_mark_field_type(
				marks,
				terms,
				field_id,
				depth + 1
			) != 0) {
			return -1;
		}
	}
	if ((constructor->result_classifier != PROTOTYPE_INVALID_ID &&
			artifact_mark_term(
				marks, terms, constructor->result_classifier, depth + 1
			) != 0) ||
		(cache->classifier != PROTOTYPE_INVALID_ID &&
		 artifact_mark_term(
			marks, terms, cache->classifier, depth + 1
		 ) != 0)) {
		return -1;
	}
	return 0;
}

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (type_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (type_id >= marks->type_count || type_id >= type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->semantic_schema.type_declarations[type_id];
	if (!artifact_type_present(type)) {
		return -1;
	}
	if (type->parameter_context == PROTOTYPE_INVALID_ID ||
		type->index_context == PROTOTYPE_INVALID_ID ||
		type->name_symbol_id < 0) {
		fprintf(
			stderr,
			"artifact closure: invalid type metadata type=%u name=%d namespace=%d parameter_context=%u index_context=%u\n",
			type_id,
			type->name_symbol_id,
			type->namespace_symbol_id,
			type->parameter_context,
			type->index_context
		);
		return -1;
	}
	if (marks->types[type_id]) {
		return 0;
	}
	marks->types[type_id] = 1;
	if (artifact_record_publication_order(
			marks->type_order,
			marks->type_count,
			&marks->ordered_type_count,
			type_id
		) != 0) {
		return -1;
	}
	if (type->formation_classifier == PROTOTYPE_INVALID_ID ||
		artifact_mark_term(
			marks, terms, type->formation_classifier, depth + 1
		) != 0) {
		fprintf(stderr, "artifact closure: type formation failed type=%u classifier=%u\n",
			type_id, type->formation_classifier);
		return -1;
	}
	if (type->first_parameter + type->parameter_count > marks->parameter_count ||
		type->first_constructor + type->constructor_count > marks->constructor_count) {
		fprintf(stderr, "artifact closure: type slice failed type=%u\n", type_id);
		return -1;
	}
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		if (artifact_mark_parameter(marks, terms, type->first_parameter + i, depth + 1) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		if (artifact_mark_constructor(
				marks,
				terms,
				type->first_constructor + i,
				depth + 1
			) != 0) {
			fprintf(stderr, "artifact closure: constructor traversal failed type=%u constructor=%u\n",
				type_id, type->first_constructor + i);
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_case(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t case_id,
	uint32_t depth
) {
	if (!marks || !terms || case_id >= marks->case_count ||
		case_id >= terms->case_count || depth > 512) {
		return -1;
	}
	if (marks->cases[case_id]) {
		return 0;
	}
	marks->cases[case_id] = 1;
	if (artifact_record_publication_order(
			marks->case_order,
			marks->case_count,
			&marks->ordered_case_count,
			case_id
		) != 0) {
		return -1;
	}
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->constructor_owner, depth + 1) != 0) {
		return -1;
	}
	if (match_case->body != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->body, depth + 1) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		uint32_t binding_id = match_case->first_binder + i;
		if (binding_id >= marks->case_binder_count) {
			return -1;
		}
		if (!marks->case_binders[binding_id]) {
			marks->case_binders[binding_id] = 1;
			if (artifact_record_publication_order(
					marks->case_binder_order,
					marks->case_binder_count,
					&marks->ordered_case_binder_count,
					binding_id
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_mark_frame(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t ih_scope_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		if (depth > 512) {
			fprintf(stderr, "artifact closure: IH frame traversal depth exceeded frame=%u depth=%u\n",
				ih_scope_id, depth);
		}
		return -1;
	}
	if (ih_scope_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (ih_scope_id >= marks->frame_count || ih_scope_id >= terms->ih_scope_count) {
		return -1;
	}
	if (marks->frames[ih_scope_id]) {
		return 0;
	}
	marks->frames[ih_scope_id] = 1;
	if (artifact_record_publication_order(
			marks->frame_order,
			marks->frame_count,
			&marks->ordered_frame_count,
			ih_scope_id
		) != 0) {
		return -1;
	}
	if (terms->ih_scopes[ih_scope_id].match_term != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(
			marks,
			terms,
			terms->ih_scopes[ih_scope_id].match_term,
			depth + 1
		) != 0) {
		return -1;
	}
	return 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		if (depth > 512) {
			fprintf(stderr, "artifact closure: term traversal depth exceeded term=%u depth=%u\n",
				term_id, depth);
		}
		return -1;
	}
	if (term_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (term_id >= marks->term_count || term_id >= terms->term_count) {
		return -1;
	}
	if (marks->terms[term_id]) {
		return 0;
	}
	marks->terms[term_id] = 1;
	if (artifact_record_publication_order(
			marks->term_order,
			marks->term_count,
			&marks->ordered_term_count,
			term_id
		) != 0) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_mark_term(marks, terms, term->as.constructor.owner, depth + 1);
		case PROTOTYPE_TERM_APP:
			if (artifact_mark_term(
					marks, terms, term->as.app.function, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.app.argument, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: APP traversal failed term=%u function=%u argument=%u\n",
					term_id, term->as.app.function, term->as.app.argument);
				return -1;
			}
			return 0;
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_mark_term(marks, terms, term->as.lambda.body, depth + 1);
		case PROTOTYPE_TERM_PI:
			return artifact_mark_term(marks, terms, term->as.pi.domain, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.pi.codomain_family, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			if (artifact_mark_term(marks, terms, term->as.match.scrutinee, depth + 1) != 0 ||
				artifact_mark_frame(marks, terms, term->as.match.ih_scope_id, depth + 1) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				if (artifact_mark_case(
						marks,
						terms,
						term->as.match.first_case + i,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			{
				for (uint32_t i = 0; i < marks->type_count; ++i) {
					const struct prototype_type_declaration* marked_type =
						&marks->type_declarations->semantic_schema.type_declarations[i];
					if (marks->types[i] && artifact_type_present(marked_type) &&
						marked_type->representation_id ==
							term->as.type_former.representation_id) {
						return 0;
					}
				}
				uint32_t representative_type_id;
				if (prototype_type_declaration_representation_type_id(
						marks->type_declarations,
						term->as.type_former.representation_id,
						&representative_type_id
					) != 0) {
					fprintf(stderr, "artifact closure: TYPE_FORMER representation failed term=%u representation=%u\n",
						term_id, term->as.type_former.representation_id);
					return -1;
				}
				return artifact_mark_type(marks, terms, representative_type_id, depth + 1);
			}
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return artifact_mark_type(marks, terms, term->as.type_declaration.type_id, depth + 1);
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (artifact_mark_type(
					marks, terms, term->as.type_view.view_type_id, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: type view declaration failed term=%u type=%u\n",
					term_id, term->as.type_view.view_type_id);
				return -1;
			}
			if (artifact_mark_term(
					marks, terms, term->as.type_view.core, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.type_view.source, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: type view payload failed term=%u core=%u(tag=%d) source=%u(tag=%d)\n",
					term_id,
					term->as.type_view.core,
					term->as.type_view.core < terms->term_count ?
						terms->terms[term->as.type_view.core].tag : -1,
					term->as.type_view.source,
					term->as.type_view.source < terms->term_count ?
						terms->terms[term->as.type_view.source].tag : -1);
				return -1;
			}
			return 0;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_mark_frame(marks, terms, term->as.induction_hypothesis.ih_scope_id, depth + 1) == 0 &&
					artifact_mark_term(marks, terms, term->as.induction_hypothesis.argument, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return artifact_mark_term(
				marks, terms, term->as.effect_operation.classifier, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return artifact_mark_term(marks, terms, term->as.computation_type.label, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.computation_type.result, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return artifact_mark_term(marks, terms, term->as.effect_row_union.left, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.effect_row_union.right, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_mark_term(marks, terms, term->as.effect_row_forall.body, depth + 1);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return artifact_mark_term(
				marks, terms, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_mark_term(marks, terms, term->as.thunk_type.computation, depth + 1);
		case PROTOTYPE_TERM_RETURN:
			return artifact_mark_term(marks, terms, term->as.return_term.value, depth + 1);
		case PROTOTYPE_TERM_THUNK:
			return artifact_mark_term(marks, terms, term->as.thunk.computation, depth + 1);
		case PROTOTYPE_TERM_FORCE:
			return artifact_mark_term(marks, terms, term->as.force.value, depth + 1);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (artifact_mark_term(
					marks, terms, term->as.computation_fold.computation, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.computation_fold.return_clause, depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (artifact_mark_term(marks, terms, clause->operation, depth + 1) != 0 ||
					artifact_mark_term(marks, terms, clause->body, depth + 1) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return artifact_mark_term(marks, terms, term->as.operation_request.operation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.argument, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.continuation, depth + 1) == 0 ? 0 : -1;
			default:
				return 0;
	}
}

static int artifact_mark_accepted_claim(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || claim_id >= judgement->claim_count) {
		return -1;
	}
	if (depth > 512) {
		fprintf(stderr,
			"artifact closure: claim dependency depth exceeded claim=%u depth=%u\n",
			claim_id, depth);
		return -1;
	}
	if (claim_id >= marks->claim_count) {
		return -1;
	}
	const struct prototype_judgement_proposition* claim_proposition =
		prototype_judgement_claim_proposition(judgement, claim_id);
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim_proposition || !claim || claim->proposition_id >=
			marks->proposition_count) {
		return -1;
	}
	if (marks->claims[claim_id] == 1) {
		return 0;
	}
	if (marks->claims[claim_id] == 2) {
		fprintf(stderr, "artifact closure: cyclic claim dependency claim=%u\n", claim_id);
		return -1;
	}
	marks->claims[claim_id] = 2;
	if (!marks->propositions[claim->proposition_id]) {
		marks->propositions[claim->proposition_id] = 1;
		if (artifact_record_publication_order(
				marks->proposition_order,
				marks->proposition_count,
				&marks->ordered_proposition_count,
				claim->proposition_id
			) != 0) {
			return -1;
		}
	}
	if (artifact_record_publication_order(
			marks->claim_order,
			marks->claim_count,
			&marks->ordered_claim_count,
			claim_id
		) != 0) {
		return -1;
	}
	if (artifact_mark_term(marks, terms, claim_proposition->subject, depth + 1) != 0 ||
		artifact_mark_term(marks, terms, claim_proposition->classifier, depth + 1) != 0 ||
		(claim_proposition->authority_id != PROTOTYPE_INVALID_ID &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
		 artifact_mark_term(
			marks, terms, claim_proposition->authority_id, depth + 1
			 ) != 0)) {
		fprintf(
			stderr,
			"artifact closure: claim term traversal failed claim=%u subject=%u(tag=%d) classifier=%u(tag=%d) authority=%d:%u\n",
			claim_id,
			claim_proposition->subject,
			claim_proposition->subject < terms->term_count ?
				terms->terms[claim_proposition->subject].tag : -1,
			claim_proposition->classifier,
			claim_proposition->classifier < terms->term_count ?
				terms->terms[claim_proposition->classifier].tag : -1,
			claim_proposition->authority_kind,
			claim_proposition->authority_id
		);
		return -1;
	}
	size_t derivation_count = 0;
	for (uint32_t i = claim->first_derivation;
		i != PROTOTYPE_INVALID_ID;
		i = judgement->derivations[i].next_same_conclusion) {
		if (i >= judgement->derivation_count ||
			judgement->derivations[i].conclusion_claim_id != claim_id) {
			return -1;
		}
		derivation_count++;
	}
	uint32_t* derivation_order = malloc(
		(derivation_count == 0 ? 1 : derivation_count) *
			sizeof(*derivation_order)
	);
	if (!derivation_order) {
		return -1;
	}
	size_t derivation_position = 0;
	for (uint32_t i = claim->first_derivation;
		i != PROTOTYPE_INVALID_ID;
		i = judgement->derivations[i].next_same_conclusion) {
		derivation_order[derivation_position++] = i;
	}
	for (size_t i = 1; i < derivation_count; ++i) {
		uint32_t candidate = derivation_order[i];
		size_t position = i;
		while (position > 0 && artifact_derivation_semantic_compare(
			&judgement->derivations[candidate],
			&judgement->derivations[derivation_order[position - 1]]
		) < 0) {
			derivation_order[position] = derivation_order[position - 1];
			position--;
		}
		derivation_order[position] = candidate;
	}
	for (size_t position = 0; position < derivation_count; ++position) {
		uint32_t i = derivation_order[position];
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (i >= marks->derivation_count) {
			free(derivation_order);
			return -1;
		}
		if (!marks->derivations[i]) {
			marks->derivations[i] = 1;
			if (artifact_record_publication_order(
					marks->derivation_order,
					marks->derivation_count,
					&marks->ordered_derivation_count,
					i
				) != 0) {
				free(derivation_order);
				return -1;
			}
		}
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			if (artifact_mark_term(
					marks, terms, derivation->rule_data.induction.match, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, derivation->rule_data.induction.motive, depth + 1
				) != 0) {
				free(derivation_order);
				return -1;
			}
		} else if ((derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
			derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) &&
			artifact_mark_term(
				marks, terms, derivation->rule_data.constructor.owner_view, depth + 1
			) != 0) {
			free(derivation_order);
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				uint32_t source_claim_id = derivation->premises[j].claim_id;
				if (source_claim_id >= judgement->claim_count) {
					free(derivation_order);
					return -1;
				}
				if (marks->claims[source_claim_id] == 2) {
					fprintf(
						stderr,
						"artifact closure: claim cycle edge claim=%u derivation=%u premise=%u\n",
						claim_id,
						i,
						source_claim_id
					);
					free(derivation_order);
					return -1;
				}
				if (artifact_mark_accepted_claim(
						marks, terms, judgement,
						source_claim_id, depth + 1
					) != 0) {
					fprintf(stderr,
						"artifact closure: premise claim traversal failed claim=%u "
						"derivation=%u premise=%u depth=%u\n",
						claim_id, i, source_claim_id, depth);
					free(derivation_order);
					return -1;
				}
			} else {
				const struct prototype_judgement_proposition* proposition =
					prototype_judgement_premise_proposition(
						judgement, &derivation->premises[j]
					);
				uint32_t proposition_id =
					derivation->premises[j].scoped_proposition_id;
				if (!proposition || proposition_id >= marks->proposition_count) {
					fprintf(stderr,
						"artifact closure: scoped premise missing claim=%u derivation=%u "
						"proposition=%u depth=%u\n",
						claim_id, i, proposition_id, depth);
					free(derivation_order);
					return -1;
				}
				if (!marks->propositions[proposition_id]) {
					marks->propositions[proposition_id] = 1;
					if (artifact_record_publication_order(
							marks->proposition_order,
							marks->proposition_count,
							&marks->ordered_proposition_count,
							proposition_id
						) != 0) {
						free(derivation_order);
						return -1;
					}
				}
				if (artifact_mark_term(
						marks, terms, proposition->subject, depth + 1
					) != 0 || artifact_mark_term(
						marks, terms, proposition->classifier, depth + 1
					) != 0 ||
					(proposition->authority_id != PROTOTYPE_INVALID_ID &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
					 artifact_mark_term(
						marks, terms, proposition->authority_id, depth + 1
					 ) != 0)) {
					fprintf(stderr,
						"artifact closure: scoped premise term traversal failed claim=%u "
						"derivation=%u proposition=%u depth=%u\n",
						claim_id, i, proposition_id, depth);
					free(derivation_order);
					return -1;
				}
			}
		}
	}
	free(derivation_order);
	if (derivation_count == 0) {
		size_t linear_count = 0;
		for (size_t i = 0; i < judgement->derivation_count; ++i) {
			if (judgement->derivations[i].conclusion_claim_id == claim_id) {
				linear_count++;
			}
		}
		fprintf(
			stderr,
			"artifact closure: accepted claim %u proposition=%u has no indexed "
			"derivation (linear=%zu kind=%d authority=%d:%u context=%u "
			"occurrence=%u subject=%u classifier=%u)\n",
			claim_id,
			claim->proposition_id,
			linear_count,
			claim_proposition->kind,
			claim_proposition->authority_kind,
			claim_proposition->authority_id,
			claim_proposition->context_id,
			claim_proposition->occurrence_id,
			claim_proposition->subject,
			claim_proposition->classifier
		);
		return -1;
	}
	marks->claims[claim_id] = 1;
	return 0;
}

static int artifact_mark_exact_relation(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t classifier
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (!claim) {
			continue;
		}
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_proposition_get(judgement, claim->proposition_id);
		if (proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->subject == subject && proposition->classifier == classifier) {
			int status = artifact_mark_accepted_claim(
				marks, terms, judgement, i, 0
			);
			if (status != 0) {
				fprintf(stderr,
					"artifact closure: exact claim traversal failed claim=%u "
					"subject=%u classifier=%u\n",
					i, subject, classifier);
			}
			return status;
		}
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (!claim) {
			continue;
		}
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_proposition_get(judgement, claim->proposition_id);
		int same_projection = 0;
		if (proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->classifier != classifier) {
			continue;
		}
		if (prototype_term_core_shape_equal(
				(struct prototype_term_db*)terms,
				proposition->subject,
				subject,
				&same_projection
			) != 0) {
			continue;
		}
		if (same_projection) {
			int status = artifact_mark_accepted_claim(
				marks, terms, judgement, i, 0
			);
			if (status != 0) {
				fprintf(stderr,
					"artifact closure: projected claim traversal failed claim=%u "
					"subject=%u projected=%u classifier=%u\n",
					i, proposition->subject, subject, classifier);
			}
			return status;
		}
	}
	return 1;
}

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (claim && prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			)->subject == subject &&
			artifact_mark_accepted_claim(marks, terms, judgement, i, 0) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_claim_present(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* claim
) {
	return prototype_judgement_proposition_get(judgement, claim->proposition_id) != NULL;
}

static int artifact_derivation_present(
	const struct prototype_judgement_derivation* derivation
) {
	return derivation && derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID;
}

static const char* artifact_optional_symbol_name(
	const struct symbol_table* symbols,
	int symbol_id
);
