void prototype_verification_db_init(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation* obligations,
	size_t obligation_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->obligations = obligations;
	db->obligation_capacity = obligation_capacity;
}

uint32_t prototype_verification_obligation_schema_version(int kind) {
	switch (kind) {
		case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
		case PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION:
			return 1;
		default:
			return 0;
	}
}

size_t prototype_verification_db_count(const struct prototype_verification_db* db) {
	return db ? db->obligation_count : 0;
}

size_t prototype_verification_db_capacity(const struct prototype_verification_db* db) {
	return db ? db->obligation_capacity : 0;
}

void prototype_verification_db_clear(struct prototype_verification_db* db) {
	if (db) {
		db->obligation_count = 0;
	}
}

const struct prototype_verification_obligation* prototype_verification_db_get(
	const struct prototype_verification_db* db,
	uint32_t obligation_id
) {
	return db && obligation_id < db->obligation_count ?
		&db->obligations[obligation_id] : NULL;
}

struct prototype_verification_obligation* prototype_verification_db_get_mutable(
	struct prototype_verification_db* db,
	uint32_t obligation_id
) {
	return db && obligation_id < db->obligation_count ?
		&db->obligations[obligation_id] : NULL;
}

int prototype_verification_db_find_occurrence(
	const struct prototype_verification_db* db,
	int kind,
	uint32_t operation,
	uint32_t* p_obligation_id
) {
	if (!db || !p_obligation_id ||
		prototype_verification_obligation_schema_version(kind) == 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (obligation && obligation->kind == kind &&
			obligation->occurrence == operation) {
			*p_obligation_id = i;
			return 0;
		}
	}
	return 1;
}

static int verification_term_reference_present(
	const struct prototype_term_db* terms,
	uint32_t term
) {
	return terms && term < terms->term_count && terms->terms[term].tag != 0;
}

int prototype_verification_db_validate(
	const struct prototype_verification_db* db,
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms
) {
	if (!db || !graph || !terms) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (!obligation ||
			obligation->schema_version !=
				prototype_verification_obligation_schema_version(obligation->kind) ||
			obligation->state < PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			obligation->state > PROTOTYPE_VERIFICATION_OBLIGATION_FAILED ||
			obligation->occurrence >= graph->occurrence_count ||
			!verification_term_reference_present(terms, obligation->core_term)) {
			return -1;
		}
		switch (obligation->kind) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
				if (obligation->computation_occurrence >= graph->occurrence_count ||
					obligation->continuation_occurrence >= graph->occurrence_count ||
					!verification_term_reference_present(
						terms, obligation->input_classifier
					) ||
					!verification_term_reference_present(
						terms, obligation->classifier_family
					) ||
					!verification_term_reference_present(terms, obligation->effect_row)) {
					return -1;
				}
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION:
				if (obligation->computation_occurrence != PROTOTYPE_INVALID_ID ||
					obligation->continuation_occurrence != PROTOTYPE_INVALID_ID ||
					obligation->continuation_binder_id != PROTOTYPE_INVALID_ID ||
					!verification_term_reference_present(
						terms, obligation->input_classifier
					) || (obligation->classifier_family != PROTOTYPE_INVALID_ID &&
					 !verification_term_reference_present(
						terms, obligation->classifier_family
					 )) || !verification_term_reference_present(
						terms, obligation->effect_row
					)) {
					return -1;
				}
				break;
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_verification_db_coverage(
	const struct prototype_verification_db* db,
	struct prototype_verification_coverage* p_coverage
) {
	if (!db || !p_coverage) {
		return -1;
	}
	memset(p_coverage, 0, sizeof(*p_coverage));
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		if (!obligation || obligation->kind <= 0 || obligation->kind >= 64) {
			return -1;
		}
		p_coverage->reachable_kind_mask |= UINT64_C(1) << obligation->kind;
		switch (obligation->state) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_PENDING:
				p_coverage->pending_count++;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED:
				p_coverage->discharged_count++;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_FAILED:
				p_coverage->failed_count++;
				break;
			default:
				return -1;
		}
		switch (obligation->kind) {
			case PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT:
				p_coverage->required_runtime_capabilities |=
					PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
					PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION:
				/* Link- or host-discharge contract; it requires no runtime
				 * handler capability merely to preserve the equation. */
				break;
			default:
				return -1;
		}
	}
	return 0;
}

uint64_t prototype_backend_default_capabilities(int backend) {
	switch (backend) {
		case PROTOTYPE_BACKEND_INTERPRETER:
			return PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
				PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH |
				PROTOTYPE_RUNTIME_CAPABILITY_HANDLER |
				PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
		case PROTOTYPE_BACKEND_C:
			/* This is the contract required of a future generated C runtime. */
			return PROTOTYPE_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER |
				PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH |
				PROTOTYPE_RUNTIME_CAPABILITY_HANDLER |
				PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
		case PROTOTYPE_BACKEND_VERILOG:
			return 0;
		default:
			return 0;
	}
}

int prototype_compile_metadata_validate_backend(
	const struct prototype_compile_metadata* metadata,
	int backend,
	uint64_t available_runtime_capabilities
) {
	if (!metadata ||
		backend < PROTOTYPE_BACKEND_INTERPRETER ||
		backend > PROTOTYPE_BACKEND_VERILOG ||
		metadata->solver_exhausted ||
		metadata->solver_incomplete_count != 0) {
		return -1;
	}
	struct prototype_verification_coverage coverage;
	if (prototype_verification_db_coverage(
			&metadata->verification, &coverage
		) != 0 || coverage.failed_count != 0 ||
		(metadata->required_runtime_capabilities &
			~available_runtime_capabilities) != 0 ||
		(coverage.required_runtime_capabilities &
			~available_runtime_capabilities) != 0) {
		return -1;
	}
	if (metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		coverage.pending_count != 0) {
		return -1;
	}
	if (backend == PROTOTYPE_BACKEND_VERILOG && coverage.pending_count != 0) {
		return -1;
	}
	if (metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_EXPLORATORY &&
		backend != PROTOTYPE_BACKEND_INTERPRETER) {
		return -1;
	}
	return 0;
}

int prototype_verification_db_add(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation obligation,
	uint32_t* p_obligation_id
) {
	uint32_t current_schema =
		prototype_verification_obligation_schema_version(obligation.kind);
	if (!db || !db->obligations ||
		current_schema == 0 ||
		(obligation.schema_version != 0 &&
			obligation.schema_version != current_schema) ||
		reserve_slot(db->obligation_count, db->obligation_capacity) != 0) {
		return -1;
	}
	obligation.schema_version = current_schema;
	if (p_obligation_id) {
		*p_obligation_id = (uint32_t)db->obligation_count;
	}
	db->obligations[db->obligation_count++] = obligation;
	return 0;
}

static int verification_db_discharge_family(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t result_classifier,
	int expected_kind
) {
	if (!db || !terms || !type_declarations || obligation_id >= db->obligation_count ||
		returned_value >= terms->term_count ||
		result_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_verification_obligation* obligation =
		prototype_verification_db_get_mutable(db, obligation_id);
	if (!obligation) {
		return -1;
	}
	if (obligation->kind != expected_kind ||
		obligation->state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
		obligation->classifier_family >= terms->term_count) {
		return -1;
	}
	uint32_t family_lambda;
	uint32_t family_application;
	uint32_t normalized_application;
	if (prototype_term_pure_family_lambda(
			terms, obligation->classifier_family, &family_lambda
		) != 0 || prototype_term_app(
			terms, family_lambda, returned_value, &family_application
		) != 0 || prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			obligation->normalization_profile,
			family_application,
			&normalized_application
		) != 0 || normalized_application >= terms->term_count) {
		obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return 0;
	}
	uint32_t expected_classifier = normalized_application;
	if (terms->terms[expected_classifier].tag == PROTOTYPE_TERM_RETURN) {
		expected_classifier = terms->terms[expected_classifier].as.return_term.value;
	}
	struct prototype_term_classifier_view expected_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, expected_classifier, &expected_view
		) != 0 || expected_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		expected_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_classifier,
			result_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_FAILED;
		return 0;
	}
	obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	return 0;
}

int prototype_verification_db_discharge_computation_fold_result(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t return_result_classifier
) {
	return verification_db_discharge_family(
		db,
		terms,
		type_declarations,
		obligation_id,
		returned_value,
		return_result_classifier,
		PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT
	);
}

enum operation_runtime_value_kind {
	OPERATION_RUNTIME_VALUE_INVALID = 0,
	OPERATION_RUNTIME_VALUE_TERM,
	OPERATION_RUNTIME_VALUE_RESOURCE,
	OPERATION_RUNTIME_VALUE_RESUMPTION
};

struct operation_runtime_resource_reference {
	uint32_t slot;
	uint32_t generation;
};

struct operation_runtime_value {
	int kind;
	union {
		uint32_t term;
		struct operation_runtime_resource_reference resource;
		uint32_t resumption;
	} as;
};

struct operation_runtime_binding {
	uint32_t binding_id;
	struct operation_runtime_value value;
};

static struct operation_runtime_value operation_runtime_term_value(uint32_t term)
{
	return (struct operation_runtime_value){
		.kind = OPERATION_RUNTIME_VALUE_TERM,
		.as.term = term
	};
}

static struct operation_runtime_value operation_runtime_resumption_value(
	uint32_t resumption
)
{
	return (struct operation_runtime_value){
		.kind = OPERATION_RUNTIME_VALUE_RESUMPTION,
		.as.resumption = resumption
	};
}
