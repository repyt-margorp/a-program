#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_GRAPH_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_GRAPH_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/ast.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

struct prototype_compile_metadata;

struct prototype_compile_label {
	int name_symbol_id;
	uint32_t term;
	uint32_t classifier;
	uint32_t operation;
	struct prototype_term_canonical_key canonical_key;
};

/*
 * Operation nodes preserve the typed/source occurrence graph produced by AST
 * lowering.  Their core_term fields may intentionally alias: for example,
 * \x : Bool => x and \y : Nat => y share one core lambda but have distinct
 * operation nodes and classifiers.
 */
enum prototype_operation_tag {
	PROTOTYPE_OPERATION_ATOM = 1,
	PROTOTYPE_OPERATION_VAR = 2,
	PROTOTYPE_OPERATION_NAME = 3,
	PROTOTYPE_OPERATION_CONSTRUCTOR = 4,
	PROTOTYPE_OPERATION_APP = 5,
	PROTOTYPE_OPERATION_LAMBDA = 6,
	PROTOTYPE_OPERATION_MATCH = 7,
	PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS = 8,
	PROTOTYPE_OPERATION_ASCRIPTION = 9,
	PROTOTYPE_OPERATION_RETURN = 10,
	PROTOTYPE_OPERATION_THUNK = 11,
	PROTOTYPE_OPERATION_FORCE = 12,
	PROTOTYPE_OPERATION_REQUEST = 13,
	PROTOTYPE_OPERATION_COMPUTATION_FOLD = 14
};

enum prototype_operation_polarity {
	PROTOTYPE_OPERATION_POLARITY_UNKNOWN = 0,
	PROTOTYPE_OPERATION_POLARITY_VALUE = 1,
	PROTOTYPE_OPERATION_POLARITY_COMPUTATION = 2
};

struct prototype_operation_node {
	int tag;
	/* This belongs to the source operation occurrence, not to the erased core
	 * term. A shared core lambda can be raw in one occurrence and thunked in
	 * another. */
	int polarity;
	int computation_kind;
	/* APP is one shared core node. This occurrence field selects the typed
	 * elimination or introduction rule without changing TermDB identity. */
	int application_role;
	/* Typed occurrences are indexed by a value context. This ID is deliberately
	 * not part of the erased TermDB node or its canonical key. */
	uint32_t context_id;
	uint32_t core_term;
	/* A lowering-time fact supplied to the solver. It is not a solved
	 * operation classifier and must never be published as one. */
	uint32_t known_classifier;
	/* The solver result for this source operation. */
	uint32_t classifier;
	/* Solver-local classifier variable. It is an operation identity, not a TermDB id. */
	uint32_t classifier_variable;
	uint32_t source_ast;
	int source_symbol_id;
	int binder_symbol_id;
	/* Source-operation binder identity for VAR occurrences. The core VAR may
	 * alias another scoped occurrence after tagless canonicalization. */
	uint32_t referenced_ast_binder_id;
	/* Graph binding introduced by a Lambda occurrence. This cannot be recovered
	 * from core_term after alpha-interning selects another representative. */
	uint32_t binding_id;
	uint32_t function;
	uint32_t argument;
	uint32_t body;
	uint32_t scrutinee;
	uint32_t binder_classifier;
	/* The return branch remains singular. Operation clauses live in the
	 * computation-fold clause arena below. */
	uint32_t fold_return_ast_binder_id;
	uint32_t fold_return_binder_id;
	/* Generated return-clause lambda occurrence. The return body remains in
	 * scrutinee for source propagation and runtime evaluation. */
	uint32_t fold_return_operation;
	/* Classifier-only row binders generalized by this lambda. They are never
	 * runtime lambda arguments. */
	uint32_t implicit_effect_row_binders[16];
	uint32_t implicit_effect_row_count;
	uint32_t first_case;
	uint32_t case_count;
	uint32_t first_fold_clause;
	uint32_t fold_clause_count;
};

struct prototype_operation_match_case {
	uint32_t body_operation;
	/* Semantic case telescope. Source binder IDs below are occurrence metadata. */
	uint32_t context_id;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	int case_label_symbol_id;
	uint32_t binder_count;
	uint32_t ast_binder_ids[16];
};

struct prototype_operation_computation_fold_clause {
	uint32_t operation_operation;
	uint32_t body_operation;
	/* Generated outer lambda which binds the request and resumption. */
	uint32_t clause_operation;
	uint32_t context_id;
	uint32_t argument_ast_binder_id;
	uint32_t argument_binder_id;
	uint32_t continuation_ast_binder_id;
	uint32_t continuation_binder_id;
};

struct prototype_operation_graph {
	struct prototype_operation_node* operations;
	size_t operation_count;
	size_t operation_capacity;
	struct prototype_operation_match_case* cases;
	size_t case_count;
	size_t case_capacity;
	struct prototype_operation_computation_fold_clause* fold_clauses;
	size_t fold_clause_count;
	size_t fold_clause_capacity;
};

int prototype_operation_graph_reaches(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation
);

enum prototype_operation_effect_constraint_kind {
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_EXACT = 1,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_COPY = 2,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNION = 3,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_RESIDUAL = 4
};

enum prototype_operation_effect_constraint_state {
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_PENDING = 1,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED = 2,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL = 3,
	PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_INCOMPLETE = 4
};

/*
 * Effect constraints are occurrence-level compiler state. Row fields are
 * TermDB ids; operation identifies the typed source occurrence whose
 * classifier owns result_row. EXACT uses left_row as the required row and
 * leaves right_row invalid.
 */
struct prototype_operation_effect_constraint {
	int kind;
	int state;
	uint32_t operation;
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
	uint32_t operation;
	uint32_t core_term;
	uint32_t computation_operation;
	uint32_t continuation_operation;
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

void prototype_operation_graph_init(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* cases,
	size_t case_capacity,
	struct prototype_operation_computation_fold_clause* fold_clauses,
	size_t fold_clause_capacity
);
size_t prototype_operation_graph_count(const struct prototype_operation_graph* graph);
size_t prototype_operation_graph_case_count(const struct prototype_operation_graph* graph);
const struct prototype_operation_node* prototype_operation_graph_get(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id
);
int prototype_operation_graph_selected_classifier(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id,
	uint32_t* p_classifier
);
const struct prototype_operation_match_case* prototype_operation_graph_get_case(
	const struct prototype_operation_graph* graph,
	uint32_t case_id
);
const struct prototype_operation_computation_fold_clause*
prototype_operation_graph_get_fold_clause(
	const struct prototype_operation_graph* graph,
	uint32_t clause_id
);
int prototype_operation_graph_add(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_node operation,
	uint32_t* p_operation_id
);
int prototype_operation_graph_add_case(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_match_case operation_case,
	uint32_t* p_case_id
);
int prototype_operation_graph_add_fold_clause(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_computation_fold_clause clause,
	uint32_t* p_clause_id
);
int prototype_operation_graph_validate(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
void prototype_compile_metadata_operation_graph(
	struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
);
void prototype_compile_metadata_operation_graph_const(
	const struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
);
void prototype_compile_metadata_commit_operation_graph(
	struct prototype_compile_metadata* metadata,
	const struct prototype_operation_graph* graph
);
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
int prototype_verification_db_find_operation(
	const struct prototype_verification_db* db,
	int kind,
	uint32_t operation,
	uint32_t* p_obligation_id
);
int prototype_verification_db_validate(
	const struct prototype_verification_db* db,
	const struct prototype_operation_graph* graph,
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
int prototype_operation_evaluate_with_verification(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state
);
enum prototype_runtime_failure_kind {
	PROTOTYPE_RUNTIME_FAILURE_NONE = 0,
	PROTOTYPE_RUNTIME_FAILURE_INVALID_OPERATION,
	PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY,
	PROTOTYPE_RUNTIME_FAILURE_UNHANDLED_OPERATION,
	PROTOTYPE_RUNTIME_FAILURE_VERIFICATION
};

struct prototype_runtime_trace {
	int failure_kind;
	uint32_t failed_operation;
	uint32_t frame_count;
	int frame_kinds[64];
	uint32_t frame_operations[64];
	uint32_t obligation_instance_count;
	int obligation_states[64];
	uint32_t obligation_operations[64];
};

int prototype_operation_evaluate_with_trace(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state,
	struct prototype_runtime_trace* p_trace
);


#endif
