#ifndef A_PROGRAM_PROTOTYPE_GRAPH_VERIFICATION_H
#define A_PROGRAM_PROTOTYPE_GRAPH_VERIFICATION_H

#include "a_program/graph/typed_occurrence_model.h"

struct prototype_compile_metadata;
struct prototype_type_declaration_db;

enum prototype_occurrence_effect_constraint_kind {
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT = 1,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY = 2,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION = 3,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL = 4
};

enum prototype_occurrence_effect_constraint_state {
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_PENDING = 1,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED = 2,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL = 3,
	PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_INCOMPLETE = 4
};

/*
 * Effect constraints are occurrence-level compiler state. Row fields are
 * TermDB ids; operation identifies the typed source occurrence whose
 * classifier owns result_row. EXACT uses left_row as the required row and
 * leaves right_row invalid.
 */
struct prototype_occurrence_effect_constraint {
	int kind;
	int state;
	uint32_t occurrence;
	uint32_t result_row;
	uint32_t left_row;
	uint32_t right_row;
};

/* Residual verification is distinct from JudgementDB: a record here is a
 * conditional runtime obligation, never a closed has-type derivation. */
enum prototype_verification_obligation_kind {
	PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT = 1
};

enum prototype_verification_obligation_state {
	PROTOTYPE_VERIFICATION_OBLIGATION_PENDING = 1,
	PROTOTYPE_VERIFICATION_OBLIGATION_DISCHARGED = 2,
	PROTOTYPE_VERIFICATION_OBLIGATION_FAILED = 3
};

struct prototype_verification_obligation {
	int kind;
	int state;
	uint32_t occurrence;
	uint32_t core_term;
	uint32_t computation_occurrence;
	uint32_t continuation_occurrence;
	uint32_t continuation_binder_id;
	uint32_t input_classifier;
	uint32_t classifier_family;
	uint32_t effect_row;
	int normalization_profile;
	uint32_t schema_version;
};

struct prototype_verification_db {
	struct prototype_verification_obligation* obligations;
	size_t obligation_count;
	size_t obligation_capacity;
};

struct prototype_verification_coverage {
	size_t pending_count;
	size_t discharged_count;
	size_t failed_count;
	uint64_t reachable_kind_mask;
	uint64_t required_runtime_capabilities;
};

uint64_t prototype_backend_default_capabilities(int backend);
int prototype_compile_metadata_validate_backend(
	const struct prototype_compile_metadata* metadata,
	int backend,
	uint64_t available_runtime_capabilities
);
void prototype_verification_db_init(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation* obligations,
	size_t obligation_capacity
);
uint32_t prototype_verification_obligation_schema_version(int kind);
size_t prototype_verification_db_count(const struct prototype_verification_db* db);
size_t prototype_verification_db_capacity(const struct prototype_verification_db* db);
void prototype_verification_db_clear(struct prototype_verification_db* db);
const struct prototype_verification_obligation* prototype_verification_db_get(
	const struct prototype_verification_db* db,
	uint32_t obligation_id
);
struct prototype_verification_obligation* prototype_verification_db_get_mutable(
	struct prototype_verification_db* db,
	uint32_t obligation_id
);
int prototype_verification_db_find_occurrence(
	const struct prototype_verification_db* db,
	int kind,
	uint32_t occurrence,
	uint32_t* p_obligation_id
);
int prototype_verification_db_validate(
	const struct prototype_verification_db* db,
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms
);
int prototype_verification_db_coverage(
	const struct prototype_verification_db* db,
	struct prototype_verification_coverage* p_coverage
);
int prototype_verification_db_add(
	struct prototype_verification_db* db,
	struct prototype_verification_obligation obligation,
	uint32_t* p_obligation_id
);
int prototype_verification_db_discharge_computation_fold_result(
	struct prototype_verification_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t obligation_id,
	uint32_t returned_value,
	uint32_t return_result_classifier
);
#endif
