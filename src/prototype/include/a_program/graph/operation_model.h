#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_MODEL_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_MODEL_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/term.h"

struct prototype_compile_label {
	int name_symbol_id;
	uint32_t term;
	/* The assignment RHS occurrence and its independently synthesized
	 * principal. For an ASCRIPTION root this is the operation below it. */
	uint32_t body_operation;
	uint32_t body_classifier;
	/* Published/evaluation view. An outer ASCRIPTION may differ from body. */
	uint32_t exposed_operation;
	uint32_t exposed_classifier;
	uint32_t expectation_classifier;
	uint32_t expectation_claim_id;
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

struct prototype_operation_node {
	int tag;
	/* This belongs to the source operation occurrence, not to the erased core
	 * term. A shared core lambda can be raw in one occurrence and thunked in
	 * another. */
	int category;
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
	/* An IH edge belongs to one exact typed Match case field. The erased Core
	 * VAR binding is alpha-canonical and cannot recover this occurrence
	 * identity. `scrutinee` and `argument` remain the owner Match and recursive
	 * argument Operation IDs respectively. */
	uint32_t ih_scope_id;
	uint32_t ih_case_index;
	uint32_t ih_field_index;
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

struct prototype_operation_induction_edge {
	uint32_t induction_operation;
	uint32_t owner_match_operation;
	uint32_t scope_id;
	uint32_t case_index;
	uint32_t field_index;
	uint32_t source_ast_binder_id;
	uint32_t argument_operation;
};

#endif
