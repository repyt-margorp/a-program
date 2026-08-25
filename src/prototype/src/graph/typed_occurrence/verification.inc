void prototype_verification_db_init(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation* obligations,
	size_t obligation_capacity,
	struct prototype_verification_dependency* dependencies,
	size_t dependency_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->obligations = obligations;
	db->obligation_capacity = obligation_capacity;
	db->dependencies = dependencies;
	db->dependency_capacity = dependency_capacity;
}

static const struct prototype_verification_kind_descriptor
	verification_kind_descriptors[] = {
		{
			.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			.schema_version = 1,
			.preservation_phase_mask = PROTOTYPE_VERIFICATION_PHASE_RUNTIME,
			.discharge_phase_mask = PROTOTYPE_VERIFICATION_PHASE_RUNTIME,
			.discharge_can_enable_claim_reconstruction = 1
		},
		{
			.kind = PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION,
			.schema_version = 2,
			.preservation_phase_mask = PROTOTYPE_VERIFICATION_PHASE_COMPILE |
				PROTOTYPE_VERIFICATION_PHASE_LINK,
			.discharge_phase_mask = PROTOTYPE_VERIFICATION_PHASE_COMPILE |
				PROTOTYPE_VERIFICATION_PHASE_LINK,
			/* The equation can close an effect condition, but the equation alone
			 * is not a typing derivation. Candidate replay is not serialized. */
			.discharge_can_enable_claim_reconstruction = 0
		}
	};

const struct prototype_verification_kind_descriptor*
prototype_verification_obligation_descriptor(int kind) {
	for (size_t i = 0; i < sizeof(verification_kind_descriptors) /
			sizeof(verification_kind_descriptors[0]); ++i) {
		if (verification_kind_descriptors[i].kind == kind) {
			return &verification_kind_descriptors[i];
		}
	}
	return NULL;
}

uint32_t prototype_verification_obligation_schema_version(int kind) {
	const struct prototype_verification_kind_descriptor* descriptor =
		prototype_verification_obligation_descriptor(kind);
	return descriptor ? descriptor->schema_version : 0;
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
		db->dependency_count = 0;
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
	if ((db->obligation_count != 0 && !db->obligations) ||
		(db->dependency_count != 0 && !db->dependencies)) {
		return -1;
	}
	uint64_t* occurrence_kind_masks = calloc(
		graph->occurrence_count ? graph->occurrence_count : 1,
		sizeof(*occurrence_kind_masks)
	);
	if (!occurrence_kind_masks) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(db, i);
		const struct prototype_verification_kind_descriptor* descriptor =
			obligation ? prototype_verification_obligation_descriptor(
				obligation->kind
			) : NULL;
		if (!obligation || !descriptor || obligation->kind < 0 ||
			obligation->kind >= 64 ||
			obligation->schema_version != descriptor->schema_version ||
			obligation->state < PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			obligation->state > PROTOTYPE_VERIFICATION_OBLIGATION_FAILED ||
			obligation->occurrence >= graph->occurrence_count ||
			!verification_term_reference_present(terms, obligation->core_term)) {
			free(occurrence_kind_masks);
			return -1;
		}
		uint64_t kind_bit = UINT64_C(1) << obligation->kind;
		if (occurrence_kind_masks[obligation->occurrence] & kind_bit) {
			free(occurrence_kind_masks);
			return -1;
		}
		occurrence_kind_masks[obligation->occurrence] |= kind_bit;
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
					free(occurrence_kind_masks);
					return -1;
				}
				break;
			case PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION:
			{
				int requires_right = obligation->effect_constraint_kind ==
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION ||
					obligation->effect_constraint_kind ==
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL;
				struct prototype_effect_row_normal_form row;
				if (obligation->computation_occurrence != PROTOTYPE_INVALID_ID ||
					obligation->continuation_occurrence != PROTOTYPE_INVALID_ID ||
					obligation->continuation_binder_id != PROTOTYPE_INVALID_ID ||
					!verification_term_reference_present(
						terms, obligation->input_classifier
					) || (obligation->classifier_family != PROTOTYPE_INVALID_ID &&
					 !verification_term_reference_present(
						terms, obligation->classifier_family
					 )) || obligation->effect_constraint_kind <
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT ||
					obligation->effect_constraint_kind >
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL ||
					!verification_term_reference_present(
						terms, obligation->effect_row
					) || obligation->normalization_profile !=
						PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
					requires_right !=
						(obligation->classifier_family != PROTOTYPE_INVALID_ID) ||
					prototype_term_effect_row_normal_form(
						terms, obligation->effect_row, &row
					) != 0 || prototype_term_effect_row_normal_form(
						terms, obligation->input_classifier, &row
					) != 0 || (requires_right &&
					 prototype_term_effect_row_normal_form(
						terms, obligation->classifier_family, &row
					 ) != 0)) {
					free(occurrence_kind_masks);
					return -1;
				}
				break;
			}
			default:
				free(occurrence_kind_masks);
				return -1;
		}
	}
	free(occurrence_kind_masks);
	for (size_t i = 0; i < db->dependency_count; ++i) {
		const struct prototype_verification_dependency* dependency =
			&db->dependencies[i];
		if (dependency->occurrence >= graph->occurrence_count ||
			dependency->obligation_id >= db->obligation_count ||
			db->obligations[dependency->obligation_id].state !=
				PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			dependency->occurrence !=
				db->obligations[dependency->obligation_id].occurrence ||
			graph->occurrences[dependency->occurrence].core_term !=
				db->obligations[dependency->obligation_id].core_term ||
			(i != 0 &&
			 (db->dependencies[i - 1].occurrence > dependency->occurrence ||
			  (db->dependencies[i - 1].occurrence == dependency->occurrence &&
			   db->dependencies[i - 1].obligation_id >= dependency->obligation_id)))) {
			return -1;
		}
	}
	for (uint32_t obligation_id = 0; obligation_id < db->obligation_count;
		++obligation_id) {
		const struct prototype_verification_obligation* obligation =
			&db->obligations[obligation_id];
		if (obligation->state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING) {
			continue;
		}
		int has_source_edge = 0;
		for (size_t dependency_id = 0;
			dependency_id < db->dependency_count;
			++dependency_id) {
			if (db->dependencies[dependency_id].obligation_id == obligation_id &&
				db->dependencies[dependency_id].occurrence == obligation->occurrence) {
				has_source_edge = 1;
				break;
			}
		}
		if (!has_source_edge) {
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
		metadata->effort.exhausted ||
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

int prototype_verification_db_add_dependency(
	struct prototype_verification_db* db,
	uint32_t occurrence,
	uint32_t obligation_id
) {
	if (!db || !db->dependencies || obligation_id >= db->obligation_count) {
		return -1;
	}
	size_t position = 0;
	while (position < db->dependency_count) {
		const struct prototype_verification_dependency* existing =
			&db->dependencies[position];
		if (existing->occurrence == occurrence &&
			existing->obligation_id == obligation_id) {
			return 0;
		}
		if (existing->occurrence > occurrence ||
			(existing->occurrence == occurrence &&
			 existing->obligation_id > obligation_id)) {
			break;
		}
		position++;
	}
	if (reserve_slot(db->dependency_count, db->dependency_capacity) != 0) {
		return -1;
	}
	memmove(
		&db->dependencies[position + 1],
		&db->dependencies[position],
		(db->dependency_count - position) * sizeof(db->dependencies[0])
	);
	db->dependencies[position] = (struct prototype_verification_dependency) {
		.occurrence = occurrence,
		.obligation_id = obligation_id
	};
	db->dependency_count++;
	return 0;
}

int prototype_verification_db_remove_obligation_dependencies(
	struct prototype_verification_db* db,
	uint32_t obligation_id
) {
	if (!db || obligation_id >= db->obligation_count) {
		return -1;
	}
	size_t write = 0;
	for (size_t read = 0; read < db->dependency_count; ++read) {
		if (db->dependencies[read].obligation_id == obligation_id) {
			continue;
		}
		db->dependencies[write++] = db->dependencies[read];
	}
	db->dependency_count = write;
	return 0;
}

static int verification_effect_row_normal_equal(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* left,
	const struct prototype_effect_row_normal_form* right
) {
	return left && right &&
		prototype_term_effect_row_normal_form_includes(terms, left, right) &&
		prototype_term_effect_row_normal_form_includes(terms, right, left);
}

static int verification_effect_row_normal_is_ground(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* row
) {
	if (!terms || !row) {
		return 0;
	}
	for (uint32_t i = 0; i < row->atom_count; ++i) {
		uint32_t atom = row->atoms[i];
		if (atom >= terms->term_count ||
			terms->terms[atom].tag != PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
			return 0;
		}
	}
	return 1;
}

static int verification_effect_row_normal_add(
	const struct prototype_term_db* terms,
	struct prototype_effect_row_normal_form* row,
	uint32_t atom
) {
	if (!terms || !row || atom >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < row->atom_count; ++i) {
		uint32_t existing = row->atoms[i];
		if (existing >= terms->term_count) {
			return -1;
		}
		const struct prototype_term* existing_term = &terms->terms[existing];
		const struct prototype_term* atom_term = &terms->terms[atom];
		if ((existing_term->tag == PROTOTYPE_TERM_EFFECT_ROW_OPERATION &&
			atom_term->tag == PROTOTYPE_TERM_EFFECT_ROW_OPERATION &&
			existing_term->as.effect_row_operation.operation_id ==
				atom_term->as.effect_row_operation.operation_id) ||
			(existing_term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
			 atom_term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
			 existing_term->as.effect_row_var.binding_id ==
				atom_term->as.effect_row_var.binding_id)) {
			return 0;
		}
	}
	uint32_t position = 0;
	while (position < row->atom_count && row->atoms[position] < atom) {
		position++;
	}
	if (position < row->atom_count && row->atoms[position] == atom) {
		return 0;
	}
	if (row->atom_count >= PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	memmove(
		&row->atoms[position + 1],
		&row->atoms[position],
		(row->atom_count - position) * sizeof(row->atoms[0])
	);
	row->atoms[position] = atom;
	row->atom_count++;
	return 0;
}

int prototype_verification_effect_row_equation_holds(
	const struct prototype_term_db* terms,
	const struct prototype_verification_obligation* obligation
) {
	if (!terms || !obligation || obligation->kind !=
			PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION) {
		return -1;
	}
	struct prototype_effect_row_normal_form actual;
	struct prototype_effect_row_normal_form expected;
	struct prototype_effect_row_normal_form right;
	if (prototype_term_effect_row_normal_form(
			terms, obligation->effect_row, &actual
		) != 0 || prototype_term_effect_row_normal_form(
			terms, obligation->input_classifier, &expected
		) != 0 || !verification_effect_row_normal_is_ground(terms, &actual) ||
		!verification_effect_row_normal_is_ground(terms, &expected)) {
		return -1;
	}
	if (obligation->effect_constraint_kind ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION ||
		obligation->effect_constraint_kind ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL) {
		if (obligation->classifier_family == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_normal_form(
				terms, obligation->classifier_family, &right
			) != 0 || !verification_effect_row_normal_is_ground(terms, &right)) {
			return -1;
		}
	}
	if (obligation->effect_constraint_kind ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION) {
		for (uint32_t i = 0; i < right.atom_count; ++i) {
			if (verification_effect_row_normal_add(
					terms, &expected, right.atoms[i]
				) != 0) {
				return -1;
			}
		}
	} else if (obligation->effect_constraint_kind ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL) {
		struct prototype_effect_row_normal_form residual;
		memset(&residual, 0, sizeof(residual));
		for (uint32_t i = 0; i < expected.atom_count; ++i) {
			uint32_t atom = expected.atoms[i];
			int handled = 0;
			if (atom < terms->term_count && terms->terms[atom].tag ==
					PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
				int operation_id = terms->terms[
					atom
				].as.effect_row_operation.operation_id;
				for (uint32_t j = 0; j < right.atom_count; ++j) {
					uint32_t handled_atom = right.atoms[j];
					if (handled_atom >= terms->term_count ||
						terms->terms[handled_atom].tag !=
							PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
						return 0;
					}
					handled = handled || terms->terms[
						handled_atom
					].as.effect_row_operation.operation_id == operation_id;
				}
			}
			if (!handled && verification_effect_row_normal_add(
					terms, &residual, atom
				) != 0) {
				return -1;
			}
		}
		expected = residual;
	}
	return verification_effect_row_normal_equal(terms, &actual, &expected);
}

int prototype_verification_db_discharge_effect_row_equation(
	struct prototype_verification_db* db,
	const struct prototype_term_db* terms,
	uint32_t obligation_id
) {
	struct prototype_verification_obligation* obligation =
		prototype_verification_db_get_mutable(db, obligation_id);
	if (!obligation || obligation->state !=
			PROTOTYPE_VERIFICATION_OBLIGATION_PENDING) {
		return -1;
	}
	int holds = prototype_verification_effect_row_equation_holds(
		terms, obligation
	);
	if (holds <= 0) {
		return holds;
	}
	obligation->state = PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED;
	return prototype_verification_db_remove_obligation_dependencies(
		db, obligation_id
	);
}

int prototype_verification_db_try_discharge_phase(
	struct prototype_verification_db* db,
	const struct prototype_term_db* terms,
	uint32_t phase,
	size_t* p_discharged_count
) {
	if (!db || !terms || (phase != PROTOTYPE_VERIFICATION_PHASE_COMPILE &&
		phase != PROTOTYPE_VERIFICATION_PHASE_LINK)) {
		return -1;
	}
	size_t discharged = 0;
	for (uint32_t obligation_id = 0; obligation_id < db->obligation_count;
		++obligation_id) {
		struct prototype_verification_obligation* obligation =
			prototype_verification_db_get_mutable(db, obligation_id);
		const struct prototype_verification_kind_descriptor* descriptor =
			obligation ? prototype_verification_obligation_descriptor(
				obligation->kind
			) : NULL;
		if (!obligation || !descriptor) {
			return -1;
		}
		if (obligation->state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			(descriptor->discharge_phase_mask & phase) == 0) {
			continue;
		}
		if (obligation->kind !=
				PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION) {
			continue;
		}
		int holds = prototype_verification_effect_row_equation_holds(
			terms, obligation
		);
		/* A symbolic or unequal row remains pending. Malformed payloads are
		 * rejected by VerificationDB validation, not reclassified as failure. */
		if (holds != 1) {
			continue;
		}
		if (prototype_verification_db_discharge_effect_row_equation(
				db, terms, obligation_id
			) != 0) {
			return -1;
		}
		discharged++;
	}
	if (p_discharged_count) {
		*p_discharged_count = discharged;
	}
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
	return prototype_verification_db_remove_obligation_dependencies(
		db, obligation_id
	);
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
